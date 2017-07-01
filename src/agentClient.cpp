#include "backend.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

agentClient::agentClient(uint32_t p_id, std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::shared_ptr<alerterManager> p_alert, Json::Value* p_cfg) : dbTools(p_db, p_l), ready(false), active(false), id(p_id), since(0), back_cfg(p_cfg),alert(p_alert) {
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
			mysqlpp::Query query = db->query("insert into c$ressources(name, origin, data_type) values(%0q:name,%1q:ori,%2q:tbl)");
			query.parse();
			query.template_defaults["name"] = name.c_str();
			query.template_defaults["ori"] = origin.c_str();
			query.template_defaults["tbl"] = tbl.c_str();
			myqExec(query, "agentClient::createRessources", "Failed to insert ressource")
		}
		uint32_t resid = getRessourceId(origin, name);
		uint32_t hostid = getHost((*i)["x-host"].asString());
		uint32_t servid = 0;
		if (resid==0) continue;
		if (i->isMember("x-service")) {
			servid = getService(hostid, (*i)["x-service"].asString());
			mysqlpp::Query q = db->query("insert into s$ressources(serv_id,res_id) values (%0:srv,%1:res) ON DUPLICATE KEY UPDATE res_id=res_id");
			q.parse();
			q.template_defaults["srv"] = servid;
			q.template_defaults["res"] = resid;
			myqExec(q, "agentClient::createRessources", "Failed to insert s$ressources")
		} else {
			mysqlpp::Query q = db->query("insert into h$ressources(host_id,res_id) values (%0:host,%1:res) ON DUPLICATE KEY UPDATE res_id=res_id");
			q.parse();
			q.template_defaults["host"] = hostid;
			q.template_defaults["res"] = resid;
			myqExec(q, "agentClient::createRessources", "Failed to insert h$ressources")
		}

		bool found=false;
		for (std::vector< std::shared_ptr<ressourceClient> >::iterator j=ressources.begin();j!=ressources.end();j++) {
			if ( (*j)->getBaseUrl() == i.key().asString() ) {
				found = true;
				(*j)->updateDefs(&(api["definitions"][tbl]["properties"]));
			}
		}

		if(!found && i->isMember("x-service")) {
			std::shared_ptr<ressourceClient> rc = std::make_shared<ressourceClient>(servid, resid, i.key().asString(), tbl, 	&(api["definitions"][tbl]["properties"]), dbp, l, alert, client);
			rc->setService((*i)["x-service"].asString());
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

void agentClient::alertFailed() {
	// TODO: alert on agent failure
}

void agentClient::updateCounter(uint32_t p_ok, uint32_t p_missing, uint32_t p_parse) {
	uint32_t fail = 0;
	uint32_t mis = p_missing;
	if ((p_missing != 0 || p_parse != 0) && p_ok == 0) {
		alertFailed();
		fail = p_missing;
		mis  = 0;
	}
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("agentClient::updateCounter", "Failed to get a connection from the pool!"); return; }

	std::chrono::duration<double, std::milli> fp_ms = std::chrono::system_clock::now().time_since_epoch();
	mysqlpp::Query query = db->query("insert into agt$history(agt_id, timestamp, failed, missing, parse, ok) values(%0:id,%1:ts,%2:fa,%3:mi,%4:pa,%5:ok) on duplicate key update failed=%2:fa, missing=%3:mi, parse=%4:pa, ok=%5:ok");
	query.parse();
	query.template_defaults["id"] = id;
	query.template_defaults["ts"] = fp_ms.count();
	query.template_defaults["fa"] = fail;
	query.template_defaults["mi"] = mis;
	query.template_defaults["pa"] = p_parse;
	query.template_defaults["ok"] = p_ok;
	myqExec(query, "agentClient::updateCounter", "Failed to update status")

	mysqlpp::Connection::thread_end();
}


void agentClient::updateApi() {
	if (!ready) {
		init();
		return; // API update will occur during init
	}
	uint32_t ok;
	uint32_t missing;
	uint32_t parse;
	std::unique_lock<std::mutex> locker(lock);
	client->resetCount(&ok, &missing, &parse);
	if(ok != 0 || missing != 0 || parse != 0)
		updateCounter(ok, missing, parse);
	if(!client->getJSON("/api/swagger.json", api)) {
		alertFailed();
		return;
	}
	createRessources();
	createTables();
	ressources.erase(std::remove_if(ressources.begin(), ressources.end(), [this](std::shared_ptr<ressourceClient> x) {
		for (Json::Value::iterator i = api["paths"].begin();i!=api["paths"].end();i++) {
			if (x->getBaseUrl() == i.key().asString())
				return false;
		}
		return true;
	}), ressources.end());
	l->info("agentClient::updateApi", "Agent "+std::to_string(id)+"("+baseurl+"), API knowledge updated");
}

void agentClient::init() {
	if (ready) return;
	bool use_ssl = false;
	// 1st build the base URL
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("agentClient::init", "Failed to get a connection from the pool!"); return; }
	mysqlpp::Query query = db->query("select host, port, pool_freq, use_ssl from c$agents where id=%0:id");
	query.parse();
	query.template_defaults["id"] = id;
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

	ready = true;
	updateApi();
}

void	agentClient::collect() {
	Json::Value data;
	std::chrono::duration<double, std::milli> fp_ms = std::chrono::system_clock::now().time_since_epoch();
	if(!client->getJSON("/all?since="+std::to_string(trunc(since)), data)) return;
	since  = data["timestamp"].asDouble();

	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("servicesClient::collectLog", "Failed to get a connection from the pool!"); return; }

	// Managing services --------------------------------------------
	std::set<uint32_t> hosts; // hold all known hosts for this agent
	for (Json::Value::iterator i = data["services"].begin();i!=data["services"].end();i++) {
		uint32_t host_id = getHost((*i)["logs"]["host"].asString());
		uint32_t serv_id = getService(host_id, i.key().asString());
		uint32_t type_id = getServiceType((*i)["status"]["properties"]["type"].asString()); 
		uint32_t failed  = 0;
		uint32_t nfailed = 0;
		uint32_t ok      = 0;
		hosts.insert(host_id);
		uint32_t maxlvl	 = 0;
		std::string t    = "";
		// Logs -------------------------------------------------
		mysqlpp::Query ql = db->query("insert into s$log_events(serv_id, timestamp, event_type, source_name, date_field, line_no, text) values (%0:srv, %1:ts, %2:et, %3q:src, %4q:dm, %5:ln, %6q:txt) ON DUPLICATE KEY UPDATE text=%6q:txt");
		ql.parse();
		for (Json::Value::iterator j = (*i)["logs"]["entries"].begin();j!=(*i)["logs"]["entries"].end();j++) {
			if(haveLogEvent(serv_id, (*j)["source"].asString(), (*j)["line_no"].asUInt(), (*j)["date_mark"].asString())) continue;
			t+=(*j)["text"].asString()+"\n";
			uint32_t event_type_id = getEventType((*j)["level"].asString());
			if (maxlvl<event_type_id) maxlvl=event_type_id;
			ql.template_defaults["srv"] = serv_id;
			ql.template_defaults["ts"]  = (*j)["timestamp"].asDouble();
			ql.template_defaults["et"]  = event_type_id;
			ql.template_defaults["src"] = (*j)["source"].asCString();
			ql.template_defaults["dm"]  = (*j)["date_mark"].asCString();
			ql.template_defaults["ln"]  = (*j)["line_no"].asUInt();
			ql.template_defaults["txt"] = (*j)["text"].asCString();
			myqExec(ql, "servicesClient::collectLog", "Failed to insert a log entry")
		}
		if (maxlvl>0)
			alert->sendLog(host_id, serv_id, maxlvl, t);

		// Process status ---------------------------------------
		mysqlpp::Query q = db->query("insert into s$process(serv_id, name, full_path, cwd, username, pid, status, timestamp) values (%0:srv, %1q:name, %2q:path, %3q:cwd, %4q:user, %5:pid, %6q:stts, %7:ts) ON DUPLICATE KEY UPDATE full_path=%2q:path, cwd=%3q:cwd, username=%4q:user, pid=%5:pid, status=%6q:stts, timestamp=%7:ts");
		q.parse();
		for (Json::Value::iterator j = (*i)["status"]["process"].begin();j!=(*i)["status"]["process"].end();j++) {
			q.template_defaults["srv"]  = serv_id;
			q.template_defaults["name"] = (*j)["name"].asCString();
			q.template_defaults["path"] = (*j)["full_path"].asCString();
			q.template_defaults["cwd"]  = (*j)["cwd"].asCString();
			q.template_defaults["user"] = (*j)["username"].asCString();
			q.template_defaults["stts"] = (*j)["status"].asCString();
			q.template_defaults["ts"]   = fp_ms.count();
			if ((*j)["status"].asString() == "ok") {
				ok++;
				q.template_defaults["pid"]  = (*j)["pid"].asInt();
			} else {
				if (!haveProcessStatus(serv_id,(*j)["name"].asString(),(*j)["status"].asString()))
					nfailed++;
				failed++;
				q.template_defaults["pid"]  = 0;
			}
			myqExec(q, "servicesClient::collect", "Failed to insert process")
		}

		// Sockets status ---------------------------------------
		mysqlpp::Query q2 = db->query("insert into s$sockets(serv_id, name, status, timestamp) values (%0:srv, %1q:name, %2q:stts, %3:ts) ON DUPLICATE KEY UPDATE status=%2q:stts, timestamp=%3:ts");
		q2.parse();
		for (Json::Value::iterator j = (*i)["status"]["sockets"].begin();j!=(*i)["status"]["sockets"].end();j++) {
			if ((*j)["status"].asString() != "ok") {
				if (!haveSocketStatus(serv_id,(*j)["name"].asString(),(*j)["status"].asString()))
					nfailed++;
				failed++;
			} else	ok++;
			q2.template_defaults["srv"]  = serv_id;
			q2.template_defaults["name"] = (*j)["name"].asCString();
			q2.template_defaults["stts"] = (*j)["status"].asCString();
			q2.template_defaults["ts"]   = fp_ms.count();
			myqExec(q2, "servicesClient::collect", "Failed to insert socket")
		}
		if (nfailed>0) {
			std::string msg="Service "+i.key().asString()+" on "+(*i)["host"].asString()+" have "+std::to_string(failed)+" failed componant";
			alert->sendService(host_id, serv_id, msg);
		}
		
		// Service type -----------------------------------------
		mysqlpp::Query qt = db->query("update s$services set type_id=%0:t where id=%1:s");
		qt.parse();
		qt.template_defaults["t"] = type_id;
		qt.template_defaults["s"] = serv_id;
		myqExec(qt, "servicesClient::collect", "Failed to update type")

		// Service status ---------------------------------------
		mysqlpp::Query p = db->query("insert into s$history(serv_id, timestamp, failed, missing, ok) values (%0:s, %1:t, %2:f, 0, %3:o) ON DUPLICATE KEY UPDATE failed=%2:f, ok=%3:o");
		p.parse();
		p.template_defaults["s"] = serv_id;
		p.template_defaults["t"] = fp_ms.count();
		p.template_defaults["f"] = failed;
		p.template_defaults["o"] = ok;
		myqExec(p, "servicesClient::collect", "Failed to insert history")
	}

	// update history for missing service on known hosts ------------
	for (std::set<uint32_t>::iterator i = hosts.begin();i!=hosts.end();i++) {
		mysqlpp::Query p = db->query("insert into s$history select s.id as serv_id, %0:t as timestamp, 0 as failed, ifnull(p.cnt,0)+ifnull(o.cnt,0) as missing, 0 as ok from s$services s left join (select serv_id, count(*) as cnt from s$process group by serv_id) p on s.id=p.serv_id left join (select serv_id, count(*) as cnt from s$sockets group by serv_id) o on s.id=o.serv_id where s.id not in (select serv_id from s$history where timestamp>=%0:t) and host_id=%1:h");
		p.parse();
		p.template_defaults["t"] = fp_ms.count();
		p.template_defaults["h"] = *i;
		myqExec(p, "servicesClient::collect", "Failed to insert missing history")
	}

	mysqlpp::Connection::thread_end();

	std::unique_lock<std::mutex> locker(lock);
	for (std::vector< std::shared_ptr<ressourceClient> >::iterator it = ressources.begin() ; it != ressources.end(); ++it) {
		std::vector<std::string> strings;
		std::istringstream base((*it)->getBaseUrl());
		std::string s;
		while (getline(base, s, '/'))
			strings.push_back(s);

		if ((*it)->isServiceSet())
			(*it)->parse(&(data["services"][(*it)->getServName()]["collectors"][strings[3]][strings[4]]));
		else
			(*it)->parse(&(data["system"][strings[2]][strings[3]]));
	}
}

void agentClient::startThread() {
	if (!ready || active) return;
	active=true;
	my_thread = std::thread ([this](){
		while(this->active) {
			collect();
			std::this_thread::sleep_for(std::chrono::seconds(pool_freq));
		}
	});
}

}
