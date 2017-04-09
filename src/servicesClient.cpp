#include "backend.h"

namespace watcheD {

void	servicesClient::init() {
	//std::cout << "servicesClient::init\n";
	// Here for futur use (if any)
}

void	servicesClient::collectLog() {
	Json::Value data;
	//TODO: support the "since" flag
	if(!client->getJSON("/service/all/log", data)) return;
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("servicesClient::collectLog", "Failed to get a connection from the pool!"); return; }

	for (Json::Value::iterator i = data.begin();i!=data.end();i++) {
		uint32_t host_id = getHost((*i)["host"].asString());
		uint32_t serv_id = getService(host_id, i.key().asString());
		uint32_t maxlvl = 0;
		std::string t="";
		for (Json::Value::iterator j = (*i)["entries"].begin();j!=(*i)["entries"].end();j++) {
			if(haveLogEvent(serv_id, (*j)["source"].asString(), (*j)["line_no"].asUInt(), (*j)["date_mark"].asString())) continue;
			t+=(*j)["text"].asString()+"\n";
			uint32_t event_type_id = getEventType((*j)["level"].asString());
			if (maxlvl<event_type_id) maxlvl=event_type_id;
			mysqlpp::Query q = db->query("insert into s$log_events(serv_id, timestamp, event_type, source_name, date_field, line_no, text) values (%0:srv, %1:ts, %2:et, %3q:src, %4q:dm, %5:ln, %6q:txt) ON DUPLICATE KEY UPDATE text=%6q:txt");
			q.parse();
			q.template_defaults["srv"] = serv_id;
			q.template_defaults["ts"]  = (*j)["timestamp"].asDouble();
			q.template_defaults["et"]  = event_type_id;
			q.template_defaults["src"] = (*j)["source"].asCString();
			q.template_defaults["dm"]  = (*j)["date_mark"].asCString();
			q.template_defaults["ln"]  = (*j)["line_no"].asUInt();
			q.template_defaults["txt"] = (*j)["text"].asCString();
			myqExec(q, "servicesClient::collectLog", "Failed to insert a log entry")
		}
		if (maxlvl>0)
			alert->sendLog(host_id, serv_id, maxlvl, t);
	}
	mysqlpp::Connection::thread_end();
}

void	servicesClient::collect() {
	Json::Value data;
	std::chrono::duration<double, std::milli> fp_ms = std::chrono::system_clock::now().time_since_epoch();

	// 1st: get the data from the client
	if(!client->getJSON("/service/all/status", data)) return; 
	
	// then insert into database
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("servicesClient::collect", "Failed to get a connection from the pool!"); return; }

	std::set<uint32_t> hosts; // hold all known host for this agent
	for (Json::Value::iterator i = data.begin();i!=data.end();i++) {
		uint32_t host_id = getHost((*i)["properties"]["host"].asString());
		uint32_t serv_id = getService(host_id, i.key().asString());
		uint32_t type_id = getServiceType((*i)["properties"]["type"].asString()); 
		uint32_t failed  = 0;
		uint32_t nfailed = 0;
		uint32_t ok      = 0;
		hosts.insert(host_id);

		// adding process
		for (Json::Value::iterator j = (*i)["process"].begin();j!=(*i)["process"].end();j++) {
			mysqlpp::Query q = db->query("insert into s$process(serv_id, name, full_path, cwd, username, pid, status, timestamp) values (%0:srv, %1q:name, %2q:path, %3q:cwd, %4q:user, %5:pid, %6q:stts, %7:ts) ON DUPLICATE KEY UPDATE full_path=%2q:path, cwd=%3q:cwd, username=%4q:user, pid=%5:pid, status=%6q:stts, timestamp=%7:ts");
			q.parse();
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

		// adding sockets
		for (Json::Value::iterator j = (*i)["sockets"].begin();j!=(*i)["sockets"].end();j++) {
			if ((*j)["status"].asString() != "ok") {
				if (!haveSocketStatus(serv_id,(*j)["name"].asString(),(*j)["status"].asString()))
					nfailed++;
				failed++;
			} else	ok++;
			mysqlpp::Query q = db->query("insert into s$sockets(serv_id, name, status, timestamp) values (%0:srv, %1q:name, %2q:stts, %3:ts) ON DUPLICATE KEY UPDATE status=%2q:stts, timestamp=%3:ts");
			q.parse();
			q.template_defaults["srv"]  = serv_id;
			q.template_defaults["name"] = (*j)["name"].asCString();
			q.template_defaults["stts"] = (*j)["status"].asCString();
			q.template_defaults["ts"]   = fp_ms.count();
			myqExec(q, "servicesClient::collect", "Failed to insert socket")
		}
		if (nfailed>0) {
			std::string msg="Service "+i.key().asString()+" on "+(*i)["host"].asString()+" have "+std::to_string(failed)+" failed componant";
			alert->sendService(host_id, serv_id, msg);
		}

		// updating service type
		mysqlpp::Query t = db->query("update s$services set type_id=%0:t where id=%1:s");
		t.parse();
		t.template_defaults["t"] = type_id;
		t.template_defaults["s"] = serv_id;
		myqExec(t, "servicesClient::collect", "Failed to update type")

		// updating status
		mysqlpp::Query p = db->query("insert into s$history(serv_id, timestamp, failed, missing, ok) values (%0:s, %1:t, %2:f, 0, %3:o) ON DUPLICATE KEY UPDATE failed=%2:f, ok=%3:o");
		p.parse();
		p.template_defaults["s"] = serv_id;
		p.template_defaults["t"] = fp_ms.count();
		p.template_defaults["f"] = failed;
		p.template_defaults["o"] = ok;
		myqExec(p, "servicesClient::collect", "Failed to insert history")
	}

	// update history for missing service on known hosts
	for (std::set<uint32_t>::iterator i = hosts.begin();i!=hosts.end();i++) {
		mysqlpp::Query p = db->query("insert into s$history select s.id as serv_id, %0:t as timestamp, 0 as failed, ifnull(p.cnt,0)+ifnull(o.cnt,0) as missing, 0 as ok from s$services s left join (select serv_id, count(*) as cnt from s$process group by serv_id) p on s.id=p.serv_id left join (select serv_id, count(*) as cnt from s$sockets group by serv_id) o on s.id=o.serv_id where s.id not in (select serv_id from s$history where timestamp>=%0:t) and host_id=%1:h");
		p.parse();
		p.template_defaults["t"] = fp_ms.count();
		p.template_defaults["h"] = *i;
		myqExec(p, "servicesClient::collect", "Failed to insert missing history")
	}

	mysqlpp::Connection::thread_end();
}

}
