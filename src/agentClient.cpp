#include "backend.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

agentClient::agentClient(uint32_t p_id, std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::shared_ptr<alerterManager> p_alert, Json::Value* p_cfg) : dbTools(p_db, p_l), ready(false), active(false), id(p_id), back_cfg(p_cfg),alert(p_alert) {
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
	if (!db) { l->error("agentClient::createRessources", "Failed to get a connection from the pool!"); return; }
	for (Json::Value::iterator i = api["paths"].begin();i!=api["paths"].end();i++) {
		std::string res = i.key().asString().substr(1, i.key().asString().rfind("/")-1);
		std::string origin = "service";
		std::string name;
		if (res.substr(0,7) == "system/") {
			origin = "host";
			name   = res.substr(7);
		} else {
			auto p = res.substr(9).find("/") + 10;
			name = res.substr(p);
		}
		if (i.key().asString().substr(i.key().asString().rfind("/")+1) != "history") continue; //ignore non-history paths
		if ( ! (*i).isMember("get") ) continue;	// ignore non-getter paths

		// TODO: should check that this $ref actually exist
		std::string ref = (*i)["get"]["responses"]["200"]["schema"]["items"]["$ref"].asString();
		std::string tbl	= ref.substr(ref.rfind("/")+1);
		if (! haveRessource(origin, name)) {
			mysqlpp::Query query = db->query("insert into c$ressources(name, origin, data_type) values('"+name+"','"+origin+"','"+tbl+"')");
			myqExec(query, "agentClient::createRessources", "Failed to insert ressource")
		}
		uint32_t resid = getRessourceId(origin, name);
		uint32_t hostid = getHost((*i)["x-host"].asString());
		uint32_t servid = 0;
		if (resid==0) continue;
		if (i->isMember("x-service")) {
			servid = getService(hostid, (*i)["x-service"].asString());
			mysqlpp::Query q = db->query("insert into s$ressources(serv_id,res_id) values ("+std::to_string(servid)+","+std::to_string(resid)+") ON DUPLICATE KEY UPDATE res_id=res_id");
			myqExec(q, "agentClient::createRessources", "Failed to insert s$ressources")
		} else {
			mysqlpp::Query q = db->query("insert into h$ressources(host_id,res_id) values ("+std::to_string(hostid)+","+std::to_string(resid)+") ON DUPLICATE KEY UPDATE res_id=res_id");
			myqExec(q, "agentClient::createRessources", "Failed to insert h$ressources")
		}

		bool found=false;
		for (std::vector< std::shared_ptr<ressourceClient> >::iterator j=ressources.begin();j!=ressources.end();j++) {
			if ( (*j)->getBaseUrl() == i.key().asString() )
				found = true;
		}

		if(!found && i->isMember("x-service")) {
			std::shared_ptr<ressourceClient> rc = std::make_shared<ressourceClient>(servid, resid, i.key().asString(), tbl, 	&(api["definitions"][tbl]["properties"]), dbp, l, alert, client);
			rc->setService();
			rc->init();
			ressources.push_back(rc);
		} else if (!found) {
			// instanciate a ressourceClient
			std::shared_ptr<ressourceClient> rc = std::make_shared<ressourceClient>(hostid, resid, i.key().asString(), tbl, 	&(api["definitions"][tbl]["properties"]), dbp, l, alert, client);
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
	std::string pk  = "host_id";
	std::string fk  = "";
	if (!db) {  l->error("agentClient::createTables", "Failed to get a connection from the pool!"); return; }
	for (Json::Value::iterator i = api["definitions"].begin();i!=api["definitions"].end();i++) {
		std::string serv = "service";
		pk  = "host_id";
		if (i->isMember("x-isService")) {
			if(!(*i)["x-isService"].asBool())
				continue; // this is the core tables defined in the schema.sql
			pk = "serv_id";
		} else if (i.key().asString().substr(0,serv.size()) == serv) {
			continue; // ignore service definitions
		}
		if ( ! haveTable("d$"+i.key().asString())) {
			std::stringstream ss;
			ss << "create table d$" << i.key().asString() << "(" << std::endl;
			if (i->isMember("x-isService")) {
				ss << "\tserv_id\tint(32) unsigned," << std::endl;
				fk = ", constraint fk_"+i.key().asString()+"_ressources foreign key(serv_id, res_id) references s$ressources(serv_id, res_id) on delete cascade on update cascade";
			} else {
				ss << "\thost_id\tint(32) unsigned," << std::endl;
				fk = ", constraint fk_"+i.key().asString()+"_ressources foreign key(host_id, res_id) references h$ressources(host_id, res_id) on delete cascade on update cascade";
			}
			ss << "\tres_id\t\tint(32) unsigned," << std::endl;
			ss << "\ttimestamp\tdouble(20,4) unsigned," << std::endl;
			for (Json::Value::iterator j = (*i)["properties"].begin();j!=(*i)["properties"].end();j++) {
				if (j.key().asString() == "timestamp") continue;
				if ((*j)["type"] == "number")
					ss << "\t" << j.key().asString() << "\t\tdouble(20,4)," << std::endl;
				else if ((*j)["type"] == "integer")
					ss << "\t" << j.key().asString() << "\t\tinteger(32)," << std::endl;
				else
					l->warning("agentClient::createTables", "Unknown datatype : "+(*j)["type"].asString());
			}
			ss << "\tconstraint " << i.key().asString() << "_pk primary key (" << pk << ",res_id,timestamp)" << std::endl;
			ss << fk << std::endl;
			ss << ")" << std::endl;
			mysqlpp::Query query = db->query(ss.str());
			myqExec(query, "agentClient::createTables", "Failed to create data table: d$"+i.key().asString())
		} else {
			// check for missing columns and add them as needed
			for (Json::Value::iterator j = (*i)["properties"].begin();j!=(*i)["properties"].end();j++) {
				if (!tableHasColumn("d$"+i.key().asString(), j.key().asString())) {
					std::stringstream ss;
					ss << "alter table d$" << i.key().asString() << " add " << j.key().asString();
					if ((*j)["type"] == "number")
						ss << " double(20,4)";
					else if ((*j)["type"] == "integer")
						ss << " integer(32)";
					else {
						l->warning("agentClient::createTables", "Unknown datatype : " + (*j)["type"].asString());
						continue;
					}
					mysqlpp::Query query = db->query(ss.str());
					myqExec(query, "agentClient::createTables", "Failed to add a column to a data table")
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
	l->info("agentClient::updateApi", "Agent "+std::to_string(id)+"("+baseurl+"), API knowledge updated");
}

void agentClient::init() {
	if (ready) return;
	bool use_ssl = false;
	// 1st build the base URL
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("agentClient::init", "Failed to get a connection from the pool!"); return; }
	mysqlpp::Query query = db->query();
	query << "select host, port, pool_freq, use_ssl from c$agents where id=" << id;
	try {
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
		l->error("agentClient::createTables", "Cannot look for agent id="+std::to_string(id));
		mysqlpp::Connection::thread_end();
		return;
	}
	} myqCatch(query, "agentClient::init","Failed to get agent configuration")
	mysqlpp::Connection::thread_end();
	client = std::make_shared<HttpClient>(baseurl, use_ssl, back_cfg, l);

	services = std::make_shared<servicesClient>(id, dbp, l, alert, client);
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
			services->collectLog();
			for (std::vector< std::shared_ptr<ressourceClient> >::iterator it = ressources.begin() ; it != ressources.end(); ++it)
				(*it)->collect();
			std::this_thread::sleep_for(std::chrono::seconds(pool_freq));
		}
	});
}

}
