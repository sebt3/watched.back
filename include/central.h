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
	
	void	sendAlert(alerter::levels p_lvl, const std::string p_message);
private:
	sel::State	state{true};
	bool		have_state;
	std::mutex	lua;
};

class alerterManager {
public:
	alerterManager(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, Json::Value* p_cfg);
	void	sendService(uint32_t p_host_id, uint32_t p_serv_id, std::string p_msg);
	void	sendLog(uint32_t p_host_id, uint32_t p_serv_id, uint32_t p_level, std::string p_lines);
	void	sendServRessource(uint32_t p_serv_id, std::shared_ptr<res_event> p_event, double p_current);
	void	sendHostRessource(uint32_t p_host_id, std::shared_ptr<res_event> p_event, double p_current);
private:
	void	send(alerter::levels p_lvl, const std::string p_message);
	std::shared_ptr<dbPool>			dbpool;
	std::shared_ptr<log>			l;
	Json::Value*				cfg;
	std::vector< std::shared_ptr<alerter> >	alerters;
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
private:
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
	dbPool(Json::Value* p_cfg) :cfg(p_cfg), usedCount(0) { }
	~dbPool() { clear(); }
	mysqlpp::Connection* grab() {
		unsigned int pool_size = (*cfg)["pool_size"].asInt();
		while (usedCount > pool_size)
			std::this_thread::sleep_for(std::chrono::seconds(1));
		++usedCount;
		return mysqlpp::ConnectionPool::grab();
	}
	void release(const mysqlpp::Connection* pc) {
		mysqlpp::ConnectionPool::release(pc);
		--usedCount;
	}
protected:
	mysqlpp::Connection* create() {
		return new mysqlpp::Connection((*cfg)["database_name"].asCString(), (*cfg)["connection_string"].asCString(), (*cfg)["login"].asCString(), (*cfg)["password"].asCString());
	}
	void destroy(mysqlpp::Connection* cp){ delete cp; }
	unsigned int max_idle_time() { return 10; }
private:
	Json::Value* cfg;
	unsigned int usedCount;

};

/*********************************
 * ressourceClient
 */
class ressourceClient : public dbTools {
public:
	ressourceClient(uint32_t p_host_id, uint32_t p_resid, std::string p_url, std::string p_table, Json::Value *p_def, std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, std::shared_ptr<alerterManager> p_alert, std::shared_ptr<HttpClient> p_client) : dbTools(p_db, p_l), isService(false), host_id(p_host_id), res_id(p_resid), baseurl(p_url), table(p_table), def(p_def), client(p_client), alert(p_alert) { }
	void	init();
	void	collect();
	std::string	getBaseUrl() { return baseurl; }
	void	setService() { isService = true; }

private:
	double  getSince();

	bool			isService;
	uint32_t		host_id;
	uint32_t		res_id;
	std::string		baseurl;
	std::string		table;
	Json::Value		*def;
	std::string		baseInsert;
	std::shared_ptr<HttpClient>				client;
	std::shared_ptr<alerterManager>				alert;
	std::vector< std::shared_ptr<struct res_event> >	event_factory;
	std::map<uint32_t, std::shared_ptr<struct res_event> >	current_events;
};

/*********************************
 * servicesClient
 */
class servicesClient : public dbTools {
public:
	servicesClient(uint32_t p_agt_id, std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::shared_ptr<alerterManager> p_alert, std::shared_ptr<HttpClient> p_client): dbTools(p_db, p_l), agt_id(p_agt_id), client(p_client), alert(p_alert) { }
	void	init();
	void	collect();
	void	collectLog();
private:
	uint32_t		agt_id;
	std::shared_ptr<HttpClient>	client;
	std::shared_ptr<alerterManager> alert;
	/*std::vector< std::shared_ptr<struct event> >		event_factory;
	std::map<uint32_t, std::shared_ptr<struct event> >	current_events;*/
};

/*********************************
 * agentClient
 */
class agentClient : public dbTools {
public:
	agentClient(uint32_t p_id, std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::shared_ptr<alerterManager> p_alert, Json::Value* p_cfg);
	~agentClient();
	void	init();
	void	updateApi();
	void	startThread();

private:
	void	createTables();
	void	createRessources();

	bool				ready;
	bool				active;
	uint32_t			id;
	std::shared_ptr<HttpClient>	client;
	std::string			baseurl;
	Json::Value			api;
	std::thread			my_thread;
	uint32_t			pool_freq;
	std::shared_ptr<servicesClient>	services;
	std::vector< std::shared_ptr<ressourceClient> >		ressources;
	Json::Value* 			back_cfg;
	std::shared_ptr<alerterManager> alert;
};

/*********************************
 * statAggregator
 */
class statAggregator : public dbTools {
public:
	statAggregator(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, Json::Value* p_aggregCfg);
	void	init();
	void	startThread();
private:
	std::map<std::string, std::string>	base_am;
	std::map<std::string, std::string>	base_ah;
	bool			active;
	std::thread		my_thread;
	Json::Value* 		cfg;
};


/*********************************
 * agentManager
 */
class agentManager : public dbTools {
public:
	agentManager(std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::shared_ptr<alerterManager> p_alert, Json::Value* p_cfg) : dbTools(p_db, p_l), back_cfg(p_cfg), alert(p_alert) { }
	void	init(Json::Value* p_aggregCfg);
	void	startThreads();
	void	updateAgents();
private:
	std::map<uint32_t, std::shared_ptr<agentClient> >	agents;
	std::shared_ptr<statAggregator>				aggreg;
	Json::Value* 						back_cfg;
	std::shared_ptr<alerterManager>				alert;
};
}

#define myqExec(q, from, msg)			\
	try {					\
	if (! q.exec()) {			\
		l->error(from,msg);		\
		l->info(from,q.error());	\
		l->notice(from,q.str());	\
	}					\
	} catch(const mysqlpp::BadQuery& er) {	\
		l->error(from,msg);		\
		l->info(from,q.error());	\
		l->notice(from,q.str());	\
	}
