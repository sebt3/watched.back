#pragma once

#include "alerter.h"

#include <mysql++/mysql++.h>

/*#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/client.h>*/

#include <thread>
#include <condition_variable>
#include <future>

#include "selene.h"
#include "client_http.hpp"
#include "client_https.hpp"

extern const std::string SERVER_HEAD;
extern const std::string APPS_NAME;
extern const std::string APPS_DESC;

namespace watcheD {

struct res_event {
	uint32_t	event_type;
	std::string	property;
	char		oper;
	double		value;
	inline bool operator==(const res_event& l) const {
		return ( (l.event_type == event_type) && (l.property == property) && (l.oper == oper) && (l.value == value) );
	}
};

/*********************************
 * Alerter Management
 */
class luaAlerter : public alerter {
public:
	luaAlerter(std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::string p_filename);
	~luaAlerter();
	
	void	sendAlert(alerter::levels p_lvl, const std::string p_dest, const std::string p_title, const std::string p_message);
private:
	sel::State	state{true};
	bool		have_state;
	std::mutex	lua;
};

class alerterManager:public dbTools {
public:
	alerterManager(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, Json::Value* p_cfg);
	void	sendService(uint32_t p_host_id, uint32_t p_serv_id, std::string p_msg);
	void	sendLog(uint32_t p_host_id, uint32_t p_serv_id, uint32_t p_level, std::string p_lines);
	void	sendServRessource(uint32_t p_serv_id, uint32_t p_res_id, std::shared_ptr<res_event> p_event, double p_current);
	void	sendHostRessource(uint32_t p_host_id, uint32_t p_res_id, std::shared_ptr<res_event> p_event, double p_current);
private:
	void	addAlerter(const std::string p_name);
	void	send(alerter::levels p_lvl, const std::string p_alerter, const std::string p_dest, const std::string p_title, const std::string p_message);
	Json::Value*				cfg;
	std::map< std::string,std::shared_ptr<alerter> >	alerters;
};


/*********************************
 * Http client
 */
typedef SimpleWeb::Client<SimpleWeb::HTTP> SWHttpClient;
typedef SimpleWeb::Client<SimpleWeb::HTTPS> SWHttpsClient;

class HttpClient {
public:
	HttpClient(std::string p_baseurl, bool p_use_ssl, Json::Value* p_cfg, std::shared_ptr<log> p_l);
	std::string request(std::string p_opt, std::string p_path);
	bool getJSON(std::string p_path, Json::Value &result);
	std::string baseURL() { return base_url; }
	void resetCount(uint32_t *p_ok, uint32_t *p_failed, uint32_t *p_parse);
private:
	uint32_t ok;
	uint32_t failed;
	uint32_t parse;
	bool use_ssl;
	std::string base_url;
	std::shared_ptr<SWHttpClient>  http;
	std::shared_ptr<SWHttpsClient> https;
	Json::Value* cfg;
	std::shared_ptr<log>	l;
};


/*********************************
 * Config
 */
class Config {
public:
	Config(std::string p_fname);
	void save();
	//Json::Value* 	getServer() { return &(data["server"]); }
	Json::Value* 	getLog();
	Json::Value* 	getDB();
	Json::Value* 	getCentral();
	Json::Value* 	getAlerter();
	Json::Value* 	getBackend() { return &(data["backend"]); }
	Json::Value* 	getAggregate();

private:
	Json::Value	data;
	std::string	fname;
};

/*********************************
 * dbPool
 */
class dbPool : public mysqlpp::ConnectionPool {
public:
	dbPool(Json::Value* p_cfg) :cfg(p_cfg), usedCount(0) {
		poolSize = (*cfg)["pool_size"].asInt();
	}
	~dbPool() { clear(); }
	mysqlpp::Connection* grab() {
		//std::cout << "dbPool GRAB--- " << usedCount << " > " << size() << std::endl;
		while (usedCount > poolSize)
			std::this_thread::sleep_for(std::chrono::seconds(1));
		++usedCount;
		return mysqlpp::ConnectionPool::grab();
	}
	void release(const mysqlpp::Connection* pc) {
		//std::cout << "dbPool release " << usedCount << " > " << size() << std::endl;
		mysqlpp::ConnectionPool::release(pc);
		--usedCount;
	}
protected:
	mysqlpp::Connection* create() {
		return new mysqlpp::Connection((*cfg)["database_name"].asCString(), (*cfg)["connection_string"].asCString(), (*cfg)["login"].asCString(), (*cfg)["password"].asCString());
	}
	void destroy(mysqlpp::Connection* cp){
		delete cp;
	}
	unsigned int max_idle_time() { return 3; }
private:
	Json::Value* cfg;
	unsigned int poolSize;
	unsigned int usedCount;

};

/*********************************
 * ressourceClient
 */
class ressourceClient : public dbTools {
public:
	ressourceClient(uint32_t p_host_id, uint32_t p_resid, std::string p_url, std::string p_table, Json::Value *p_def, std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, std::shared_ptr<alerterManager> p_alert, std::shared_ptr<HttpClient> p_client) : dbTools(p_db, p_l, "ressourceClient"), isService(false), host_id(p_host_id), res_id(p_resid), baseurl(p_url), table(p_table), servName(""), def(p_def), client(p_client), alert(p_alert) { }
	void		init(mysqlpp::Connection *p_db);
	void		parse(mysqlpp::Connection *p_db, Json::Value *p_data);
	std::string	getBaseUrl() { return baseurl; }
	std::string	getServName() { return servName; }
	void		setService(std::string n) { servName=n; isService = true; }
	void		updateDefs(Json::Value *p_def);
	bool		isServiceSet() {return isService; };
private:
	bool			isService;
	uint32_t		host_id;
	uint32_t		res_id;
	std::string		baseurl;
	std::string		table;
	std::string		servName;
	Json::Value		*def;
	std::string		baseInsert;
	std::shared_ptr<HttpClient>				client;
	std::shared_ptr<alerterManager>				alert;
	std::vector< std::shared_ptr<struct res_event> >	event_factory;
	std::map<uint32_t, std::shared_ptr<struct res_event> >	current_events;
};

/*********************************
 * agentClient
 */
class agentClient : public dbTools {
public:
	agentClient(uint32_t p_id, std::shared_ptr< watcheD::dbPool > p_db, std::shared_ptr< watcheD::log > p_l, std::shared_ptr< watcheD::alerterManager > p_alert, Json::Value* p_cfg);
	~agentClient();
	void	init();
	void	updateApi();
	void	startThread();

private:
	void	createTables();
	void	createRessources();
	void	updateCounter(uint32_t p_ok, uint32_t p_failed, uint32_t p_parse);
	void	alertFailed();
	void	collect();

	bool				ready;
	bool				active;
	uint32_t			id;
	double				since;
	uint16_t			collectCount;
	std::shared_ptr<HttpClient>	client;
	std::string			baseurl;
	Json::Value			api;
	std::thread			my_thread;
	uint32_t			pool_freq;
	std::vector< std::shared_ptr<ressourceClient> >		ressources;
	Json::Value* 			back_cfg;
	std::shared_ptr<alerterManager> alert;
//	std::mutex			lock;
};

/*********************************
 * statAggregator
 */
struct aggreg_data {
	uint32_t	delay_am;
	uint32_t	delay_ah;
	uint32_t	delay_ad;
	uint32_t	retention_d;
	uint32_t	retention_am;
	uint32_t	retention_ah;
	uint32_t	retention_ad;
	std::string	base_am;
	std::string	base_ah;
	std::string	base_ad;
	std::chrono::duration<double, std::milli>	next_low;
	std::chrono::duration<double, std::milli>	next_med;
	std::chrono::duration<double, std::milli>	next_high;
};
class statAggregator : public dbTools {
public:
	statAggregator(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, Json::Value* p_aggregCfg);
	void	init();
	void	startThread();
private:
	std::map<std::string, std::shared_ptr<aggreg_data>>	tables;
	uint32_t		thread_freq;
	bool			active;
	std::thread		my_thread;
	Json::Value* 		cfg;
};


/*********************************
 * agentManager
 */
class agentManager : public dbTools {
public:
	agentManager(std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::shared_ptr<alerterManager> p_alert, Json::Value* p_cfg) : dbTools(p_db, p_l, "agentManager"), back_cfg(p_cfg), alert(p_alert), back_id(0) { }
	void	init(Json::Value* p_aggregCfg, std::string p_me, std::string p_cfgfile);
	void	startThreads();
	void	updateAgents();
private:
	std::map<uint32_t, std::shared_ptr<agentClient> >	agents;
	std::shared_ptr<statAggregator>				aggreg;
	Json::Value* 						back_cfg;
	std::shared_ptr<alerterManager>				alert;
	int32_t							back_id;
};
}

#define myqExec(q, from, msg)			\
	try {					\
	if (! q.exec()) {			\
		l->error(from,msg);		\
		l->notice(from,q.error());	\
		l->info(from,q.str());		\
	}					\
	} catch(const mysqlpp::BadQuery& er) {	\
		l->error(from,msg);		\
		l->notice(from,q.error());	\
		l->info(from,q.str());		\
	} catch ( const std::exception& e ) {	\
		l->error(from,msg);		\
		l->notice(from,e.what());	\
	}

#define myqCatch(q,from,msg)			\
	catch(const mysqlpp::BadQuery& er) {	\
		l->error(from,msg);		\
		l->notice(from,q.error());	\
		l->info(from,q.str());		\
	} catch ( const std::exception& e ) {	\
		l->error(from,msg);		\
		l->notice(from,e.what());	\
	}
