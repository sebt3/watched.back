#pragma once

#include "common.h"

#include <mysql++/mysql++.h>

/*#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/client.h>*/

extern const std::string SERVER_HEAD;
extern const std::string APPS_NAME;
extern const std::string APPS_DESC;

namespace watcheD {

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
	
	
class dbTools {
public:
	dbTools(std::shared_ptr<dbPool>	p_db) : dbp(p_db) { }
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
	ressourceClient(uint32_t p_host_id, uint32_t p_resid, std::string p_url, std::string p_table, Json::Value *p_def, std::shared_ptr<dbPool>	p_db, std::shared_ptr<HttpClient> p_client) : dbTools(p_db), host_id(p_host_id), res_id(p_resid), baseurl(p_url), table(p_table), def(p_def), client(p_client) { }
	void	init();
	void	collect();

private:
	double  getSince();

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
	servicesClient(uint32_t p_agt_id, std::shared_ptr<dbPool> p_db, std::shared_ptr<HttpClient> p_client): dbTools(p_db), agt_id(p_agt_id), client(p_client) { }
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
	agentClient(uint32_t p_id, std::shared_ptr<dbPool> p_db);
	~agentClient();
	void	init();
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
};

/*********************************
 * statAggregator
 */
class statAggregator : public dbTools {
public:
	statAggregator(std::shared_ptr<dbPool>	p_db, Json::Value* p_aggregCfg);
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
	agentManager(std::shared_ptr<dbPool> p_db) : dbTools(p_db) { }
	void	init(Json::Value* p_aggregCfg);
	void	startThreads();
private:
	std::map<uint32_t, std::shared_ptr<agentClient> >	agents;
	std::shared_ptr<statAggregator>				aggreg;
};
}
