#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

agentClient::agentClient(uint32_t p_id, std::shared_ptr<dbPool> p_db, Json::Value* p_cfg) : dbTools(p_db), ready(false), active(false), id(p_id), back_cfg(p_cfg) {
}

agentClient::~agentClient() {
	if (active) {
		active=false;
		my_thread.join();
	}
}

void agentClient::createRessources() {
	// use the paths in the API definition to find the ressources
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	for (Json::Value::iterator i = api["paths"].begin();i!=api["paths"].end();i++) {
		std::string res = i.key().asString().substr(1, i.key().asString().rfind("/")-1);
		if (i.key().asString().substr(i.key().asString().rfind("/")+1) != "history") continue; //ignore non-history paths
		if ( ! (*i).isMember("get") ) continue;	// ignore non-getter paths

		// TODO: should check that this $ref actually exist
		std::string ref = (*i)["get"]["responses"]["200"]["schema"]["items"]["$ref"].asString();
		std::string tbl	= ref.substr(ref.rfind("/")+1);
		if (! haveRessource(res)) {
			mysqlpp::Query query = db->query("insert into ressources(name, type) values('"+res+"','"+tbl+"')");
			myqExec(query, "Failed to insert ressource")
		}
		uint32_t resid = getRessourceId(res);
		uint32_t hostid = getHost((*i)["x-host"].asString());
		if (resid==0) continue;

		mysqlpp::Query q = db->query("insert into host_ressources(host_id,res_id) values ("+std::to_string(hostid)+","+std::to_string(resid)+") ON DUPLICATE KEY UPDATE res_id=res_id");
		myqExec(q, "Failed to insert host_resource")

		bool found=false;
		for (std::vector< std::shared_ptr<ressourceClient> >::iterator j=ressources.begin();j!=ressources.end();j++) {
			if ( (*j)->getBaseUrl() == i.key().asString() )
				found = true;
		}

		if(!found) {
			// instanciate a ressourceClient
			std::shared_ptr<ressourceClient> rc = std::make_shared<ressourceClient>(hostid, resid, i.key().asString(), tbl, 	&(api["definitions"][tbl]["properties"]), dbp, client);
			rc->init();
			ressources.push_back(rc);
		}
	}
	mysqlpp::Connection::thread_end();
}

void agentClient::createTables() {
	// use the data definition part of the API to build tables to store data
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	for (Json::Value::iterator i = api["definitions"].begin();i!=api["definitions"].end();i++) {
		std::string serv = "service";
		if (i.key().asString().substr(0,serv.size()) == serv) {
			//TODO: Create tables for services too
			continue; // ignore service definitions
		}
		if ( ! haveTable(i.key().asString())) {
			std::stringstream ss;
			ss << "create table " << i.key().asString() << "(" << std::endl;
			ss << "\thost_id\tint(32) unsigned," << std::endl;
			ss << "\tres_id\t\tint(32) unsigned," << std::endl;
			ss << "\ttimestamp\tdouble(20,4) unsigned," << std::endl;
			for (Json::Value::iterator j = (*i)["properties"].begin();j!=(*i)["properties"].end();j++) {
				if (j.key().asString() == "timestamp") continue;
				if ((*j)["type"] == "number")
					ss << "\t" << j.key().asString() << "\t\tdouble(20,4)," << std::endl;
				else if ((*j)["type"] == "integer")
					ss << "\t" << j.key().asString() << "\t\tinteger(32)," << std::endl;
				else
					std::cerr << "Unknown datatype : " << (*j)["type"] << std::endl;
			}
			ss << "\tconstraint " << i.key().asString() << "_pk primary key (host_id,res_id,timestamp)" << std::endl;
			ss << ")" << std::endl;
			mysqlpp::Query query = db->query(ss.str());
			myqExec(query, "Failed to create data table")
		} else {
			// check for missing columns and add them as needed
			for (Json::Value::iterator j = (*i)["properties"].begin();j!=(*i)["properties"].end();j++) {
				if (!tableHasColumn(i.key().asString(), j.key().asString())) {
					std::stringstream ss;
					ss << "alter table " << i.key().asString() << " add " << j.key().asString();
					if ((*j)["type"] == "number")
						ss << " double(20,4)";
					else if ((*j)["type"] == "integer")
						ss << " integer(32)";
					else {
						std::cerr << "Unknown datatype : " << (*j)["type"] << std::endl;
						continue;
					}
					mysqlpp::Query query = db->query(ss.str());
					myqExec(query, "Failed to add a column to a data table")
				}
			}
			
		}
	}
	mysqlpp::Connection::thread_end();
}

void agentClient::updateApi() {
	if (!ready) {
		init();
		return; // API update will occur during init
	}
	if(!client->getJSON("/api/swagger.json", api)) return;
	createRessources();
	createTables();
	std::cout << "Agent "<< id <<"("<< baseurl << "), API knowledge updated\n";
}

void agentClient::init() {
	if (ready) return;
	bool use_ssl = false;
	// 1st build the base URL
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	mysqlpp::Query query = db->query();
	query << "select host, port, pool_freq, use_ssl from agents where id=" << id;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		if (row[0] == "localhost") {
			baseurl = "127.0.0.1:";
		} else {
			baseurl = row[0].c_str();
			baseurl.append(":");
		}
		baseurl.append(row[1].c_str());
		pool_freq = row[2];
		if (atoi(row[3].c_str())==1)
			use_ssl=true;
	}
	else {
		std::cerr << "Cannot look for agent id=" <<id << std::endl;
		mysqlpp::Connection::thread_end();
		return;
	}
	mysqlpp::Connection::thread_end();
	client = std::make_shared<HttpClient>(baseurl, use_ssl, back_cfg);

	services = std::make_shared<servicesClient>(id, dbp, client);
	services->init();

	ready = true;
	updateApi();
}

void agentClient::startThread() {
	if (!api.isMember("paths"))
		updateApi();
	if (!ready || active) return;
	active=true;
	my_thread = std::thread ([this](){
		while(this->active) {
			services->collect();
			for (std::vector< std::shared_ptr<ressourceClient> >::iterator it = ressources.begin() ; it != ressources.end(); ++it)
				(*it)->collect();
			std::this_thread::sleep_for(std::chrono::seconds(pool_freq));
		}
	});
}

}
