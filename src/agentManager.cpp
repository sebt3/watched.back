#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

void	agentManager::init(Json::Value* p_aggregCfg) {
	// TODO: check for table agents and ressources, create (empty) if not found
	aggreg  = std::make_shared<statAggregator>(dbp, p_aggregCfg);
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
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	mysqlpp::Query query = db->query("select id from agents");
	if (mysqlpp::StoreQueryResult res = query.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator it= res.begin(); it != res.end(); ++it) {
			mysqlpp::Row row = *it;
			if (agents.find(row[0]) == agents.end()) { // do not start exiting agent
				agents[row[0]] = std::make_shared<agentClient>(row[0], dbp);
				agents[row[0]]->init();
			}
		}
	}
	mysqlpp::Connection::thread_end();
}

}
