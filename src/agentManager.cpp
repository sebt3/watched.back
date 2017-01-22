#include "backend.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

void	agentManager::init(Json::Value* p_aggregCfg) {
	// TODO: check for table agents and ressources, create (empty) if not found
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
	std::string qstr = "select id from c$agents";
	int domains = (*back_cfg)["central_id"].asInt();
	if (domains>0)
		qstr = "select id from c$agents where central_id="+std::to_string(domains);
	mysqlpp::Query query = db->query(qstr);
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator it= res.begin(); it != res.end(); ++it) {
			mysqlpp::Row row = *it;
			if (agents.find(row[0]) == agents.end()) { // do not start exiting agent
				agents[row[0]] = std::make_shared<agentClient>(row[0], dbp, l, alert, back_cfg);
				agents[row[0]]->init();
				count++;
			} // TODO: request for an updated API for known agents
		}
		l->info("agentManager::updateAgents","Started "+std::to_string(count)+" agents");
	} else l->error("agentManager::updateAgents","Could not query for agent list");
	} myqCatch(query, "agentManager::updateAgents","Failed to get agents list")
	mysqlpp::Connection::thread_end();
}

}
