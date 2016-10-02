#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

using namespace watcheD;

void	agentManager::init(Json::Value* p_aggregCfg) {
	// TODO: check for table agents and ressources, create (empty) if not found
	aggreg  = new statAggregator(dbp, p_aggregCfg);
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	mysqlpp::Query query = db->query("select id from agents");
	if (mysqlpp::StoreQueryResult res = query.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator it= res.begin(); it != res.end(); ++it) {
			mysqlpp::Row row = *it;
			agents[row[0]] = new agentClient(row[0], dbp);
			agents[row[0]]->init();
		}
	}
	mysqlpp::Connection::thread_end();
	std::this_thread::sleep_for(std::chrono::seconds(2)); // give enough time for the agentClients to be ready
	aggreg->init();
}

void	agentManager::startThreads() {
	for(std::map<uint32_t, agentClient*>::iterator agtit = agents.begin();agtit != agents.end(); agtit++)
		agents[agtit->first]->startThread();
	aggreg->startThread();
}
