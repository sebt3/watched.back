#include "backend.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>
#include <limits.h>

namespace watcheD {

std::string getCurrentHost() {
	struct addrinfo hints, *info;
	std::string host = "";
	int gai_result;
	char ghost[1024];
	memset(ghost,0,1024);
	gethostname(ghost,1023);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	if ((gai_result = getaddrinfo(ghost, "http", &hints, &info)) != 0)
		return "";
	host = info->ai_canonname;
	freeaddrinfo(info);
	return host;
}

void	agentManager::init(Json::Value* p_aggregCfg, std::string p_me, std::string p_cfgfile) {
	std::string full = p_cfgfile;
	std::string me = getCurrentHost();
	if(me=="") me = p_me;
	char buf[PATH_MAX + 1];
	char *res = realpath(p_cfgfile.c_str(), buf);
	if(res)
		full = buf;
	back_id = getBackend(me, full);
	aggreg  = std::make_shared<statAggregator>(dbp, l, p_aggregCfg);
	updateAgents();
	std::this_thread::sleep_for(std::chrono::seconds(2)); // give enough time for the agentClients to be ready
	aggreg->init();
}

void	agentManager::startThreads() {
	for(std::map<uint32_t, std::shared_ptr<agentClient> >::iterator agtit = agents.begin();agtit != agents.end(); agtit++)
		agents[agtit->first]->startThread();
	aggreg->startThread();
}

void	agentManager::updateAgents() {
	uint16_t count=0;
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("agentManager::updateAgents", "Failed to get a connection from the pool!"); return; }
	int domains = (*back_cfg)["central_id"].asInt();
	mysqlpp::Query query = db->query("select id from c$agents");
	if (domains>0) {
		query << " where central_id=%0:id";
		query.parse();
		query.template_defaults["id"] = domains;
	}
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator it= res.begin(); it != res.end(); ++it) {
			mysqlpp::Row row = *it;
			if (agents.find(row[0]) == agents.end()) { // do not start exiting agent
				agents[row[0]] = std::make_shared<agentClient>(row[0], dbp, l, alert, back_cfg);
				agents[row[0]]->init();
				count++;
			} else
				agents[row[0]]->updateApi();
		}
		l->info("agentManager::updateAgents","Started "+std::to_string(count)+" agents");
	} else l->error("agentManager::updateAgents","Could not query for agent list");
	} myqCatch(query, "agentManager::updateAgents","Failed to get agents list")

	// updating self status
	std::chrono::duration<double, std::milli> fp_ms = std::chrono::system_clock::now().time_since_epoch();
	mysqlpp::Query q = db->query("insert into b$history(back_id, timestamp, failed, ok) values(%0:id, %1:ts, 0, 1) on duplicate key update failed=0, ok=1");
	q.parse();
	q.template_defaults["id"] = back_id;
	q.template_defaults["ts"] = fp_ms.count();
	myqExec(q, "agentManager::updateAgents", "Failed to insert self status")

	// looking for failed backend
	mysqlpp::Query f = db->query("insert into b$history select back_id, %0:ts-300000 timestamp, 1 failed, 0 ok from (select max(timestamp) ts, back_id from b$history group by back_id) x where x.ts<%0:ts-600000");
	f.parse();
	f.template_defaults["ts"] = fp_ms.count();
	myqExec(f, "agentManager::updateAgents", "Failed to insert failed backends status")
	// TODO: alert for failed backend
	
	mysqlpp::Connection::thread_end();
}

}
