#pragma once

#include <mysql++/mysql++.h>

/*#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/client.h>*/

#include <json/json.h>

#include <map>
#include <string>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>

#include "client_http.hpp"
#include "client_https.hpp"

extern const std::string SERVER_HEAD;
extern const std::string APPS_NAME;
extern const std::string APPS_DESC;

namespace watcheD {

class log {
public:
	log(Json::Value* p_cfg);
	void write(uint16_t p_lvl, const std::string p_src, std::string p_message);
	void write(std::string p_lvl, const std::string p_src, std::string p_message);
	void error(const std::string p_src, std::string p_message);
	void warning(const std::string p_src, std::string p_message);
	void info(const std::string p_src, std::string p_message);
	void notice(const std::string p_src, std::string p_message);
	void debug(const std::string p_src, std::string p_message);
private:
	Json::Value*	cfg;
	uint16_t	level;
	static const std::vector<std::string> levels;
	std::mutex	mutex;
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
 * dbTools
 */
class dbTools {
public:
	dbTools(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l) : dbp(p_db), l(p_l) { }
protected:
	bool		haveTable(std::string p_name);
	bool		tableHasColumn(std::string p_name, std::string p_col);
	bool		haveRessource(std::string p_name);
	uint32_t	getRessourceId(std::string p_res);
	bool		haveHost(std::string p_host_name);
	uint32_t	getHost(std::string p_host_name);
	bool		haveService(uint32_t p_host_id, std::string p_service);
	uint32_t	getService(uint32_t p_host_id, std::string p_service);
	std::shared_ptr<dbPool>	dbp;
	std::shared_ptr<log>	l;
};

/*********************************
 * ressourceClient
 */
struct res_event {
	uint32_t	event_type;
	std::string	property;
	char		oper;
	double		value;
	inline bool operator==(const res_event& l) const {
		return ( (l.event_type == event_type) && (l.property == property) && (l.oper == oper) && (l.value == value) );
	}
};

class ressourceClient : public dbTools {
public:
	ressourceClient(uint32_t p_host_id, uint32_t p_resid, std::string p_url, std::string p_table, Json::Value *p_def, std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, std::shared_ptr<HttpClient> p_client) : dbTools(p_db, p_l), isService(false), host_id(p_host_id), res_id(p_resid), baseurl(p_url), table(p_table), def(p_def), client(p_client) { }
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
	std::vector< std::shared_ptr<struct res_event> >	event_factory;
	std::map<uint32_t, std::shared_ptr<struct res_event> >	current_events;
};

/*********************************
 * servicesClient
 */
class servicesClient : public dbTools {
public:
	servicesClient(uint32_t p_agt_id, std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::shared_ptr<HttpClient> p_client): dbTools(p_db, p_l), agt_id(p_agt_id), client(p_client) { }
	void	init();
	void	collect();
private:
	uint32_t		agt_id;
	std::shared_ptr<HttpClient>				client;
	/*std::vector< std::shared_ptr<struct event> >		event_factory;
	std::map<uint32_t, std::shared_ptr<struct event> >	current_events;*/
};

/*********************************
 * agentClient
 */
class agentClient : public dbTools {
public:
	agentClient(uint32_t p_id, std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, Json::Value* p_cfg);
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
	agentManager(std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, Json::Value* p_cfg) : dbTools(p_db, p_l), back_cfg(p_cfg) { }
	void	init(Json::Value* p_aggregCfg);
	void	startThreads();
	void	updateAgents();
private:
	std::map<uint32_t, std::shared_ptr<agentClient> >	agents;
	std::shared_ptr<statAggregator>				aggreg;
	Json::Value* 						back_cfg;
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
