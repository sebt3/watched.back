#include "central.h"

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
			mysqlpp::Query q = db->query();
			q << "insert into s$log_events(serv_id,timestamp,event_type,source_name,date_field,line_no,text) values (" 
				<< serv_id << "," << (*j)["timestamp"].asDouble() << ","
				<< event_type_id << "," << mysqlpp::quote << (*j)["source"].asString() << "," 
				<< mysqlpp::quote << (*j)["date_mark"].asString() << ","
				<< (*j)["line_no"].asUInt() << "," << mysqlpp::quote << (*j)["text"].asString()
				<< ") ON DUPLICATE KEY UPDATE text="
				<< mysqlpp::quote << (*j)["text"].asString();
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
			std::string querystr;
			if ((*j)["status"].asString() == "ok") {
				ok++;
				querystr = "insert into s$process(serv_id,name,full_path,cwd,username,pid,status,timestamp) values ("+std::to_string(serv_id)+
					",'"+(*j)["name"].asString()+"','"+(*j)["full_path"].asString()+
					"','"+(*j)["cwd"].asString()+"','"+(*j)["username"].asString()+
					"',"+(*j)["pid"].asString()+",'"+(*j)["status"].asString()+
					"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE full_path='"+(*j)["full_path"].asString()+
					"',cwd='"+(*j)["cwd"].asString()+"',username='"+(*j)["username"].asString()+
					"',pid="+(*j)["pid"].asString()+",status='"+(*j)["status"].asString()+"',timestamp="+std::to_string(fp_ms.count());
			} else {
				if (!haveProcessStatus(serv_id,(*j)["name"].asString(),(*j)["status"].asString())) nfailed++;
				failed++;
				querystr = "insert into s$process(serv_id,name,full_path,cwd,username,pid,status,timestamp) values ("+std::to_string(serv_id)+
					",'"+(*j)["name"].asString()+"','"+(*j)["full_path"].asString()+
					"','"+(*j)["cwd"].asString()+"','"+(*j)["username"].asString()+
					"',0,'"+(*j)["status"].asString()+
					"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE full_path='"+(*j)["full_path"].asString()+
					"',pid=0,status='"+(*j)["status"].asString()+"',timestamp="+std::to_string(fp_ms.count());
			}
			mysqlpp::Query q = db->query(querystr);
			myqExec(q, "servicesClient::collect", "Failed to insert process")
		}

		// adding sockets
		for (Json::Value::iterator j = (*i)["sockets"].begin();j!=(*i)["sockets"].end();j++) {
			//std::cout << (*j)["name"] << std::endl;
			if ((*j)["status"].asString() != "ok") {
				if (!haveSocketStatus(serv_id,(*j)["name"].asString(),(*j)["status"].asString())) nfailed++;

				failed++;
			} else	ok++;

			mysqlpp::Query q = db->query(
				"insert into s$sockets(serv_id,name,status,timestamp) values ("+std::to_string(serv_id)+
				",'"+(*j)["name"].asString()+"','"+(*j)["status"].asString()+
				"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE status='"+(*j)["status"].asString()+
				"',timestamp="+std::to_string(fp_ms.count()));
			myqExec(q, "servicesClient::collect", "Failed to insert socket")
		}
		if (nfailed>0) {
			std::string msg="Service "+i.key().asString()+" on "+(*i)["host"].asString()+" have "+std::to_string(failed)+" failed componant";
			alert->sendService(host_id, serv_id, msg);
		}

		// updating service type
		mysqlpp::Query t = db->query(
			"update s$services set type_id="+std::to_string(type_id)+" where id="+std::to_string(serv_id)
		);
		myqExec(t, "servicesClient::collect", "Failed to update type")

		// updating status
		mysqlpp::Query p = db->query(
			"insert into s$history(serv_id,timestamp,failed,missing,ok) values ("+std::to_string(serv_id)+	","+std::to_string(fp_ms.count())+","+std::to_string(failed)+",0,"+std::to_string(ok)+") ON DUPLICATE KEY UPDATE failed="+std::to_string(failed)+",ok="+std::to_string(ok)
		);
		myqExec(p, "servicesClient::collect", "Failed to insert history")
	}

	// update history for missing service on known hosts
	for (std::set<uint32_t>::iterator i = hosts.begin();i!=hosts.end();i++) {
		mysqlpp::Query p = db->query(
			"insert into s$history select s.id as serv_id, "+std::to_string(fp_ms.count())+" as timestamp, 0 as failed, ifnull(p.cnt,0)+ifnull(o.cnt,0) as missing, 0 as ok from s$services s left join (select serv_id, count(*) as cnt from s$process group by serv_id) p on s.id=p.serv_id left join (select serv_id, count(*) as cnt from s$sockets group by serv_id) o on s.id=o.serv_id where s.id not in (select serv_id from s$history where timestamp>="+std::to_string(fp_ms.count())+") and host_id="+std::to_string(*i)
		);
		myqExec(p, "servicesClient::collect", "Failed to insert missing history")
	}

	mysqlpp::Connection::thread_end();
}

}
