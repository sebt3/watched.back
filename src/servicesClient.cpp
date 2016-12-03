#include "central.h"

namespace watcheD {

void	servicesClient::init() {
	//std::cout << "servicesClient::init\n";
	// Here for futur use (if any)
}

void	servicesClient::collect() {

	// 1st: get the data from the client
	Json::Value data;
	std::string resp;
	std::stringstream ss;
	std::chrono::duration<double, std::milli> fp_ms = std::chrono::system_clock::now().time_since_epoch();
	
	try {
		resp = client->request("GET", "/service/all/status");
	} catch (std::exception &e) {
		// retrying to cope with "read_until: End of file" errors
		try {
			resp = client->request("GET", "/service/all/status");
		} catch (std::exception &e) {
			std::cerr << "Failed to get /service/all/status after a retry:" << e.what() << std::endl;
			return;
		}
	}
	ss << resp;
	try {
		ss >> data;
	} catch(const Json::RuntimeError &er) {
		std::cerr << "Json parse failed for url : /service/all/status\n" ;
		return;
	}
	
	// then insert into database
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }

	std::set<uint32_t> hosts; // hold all known host for this agent
	for (Json::Value::iterator i = data.begin();i!=data.end();i++) {
		uint32_t host_id = getHost((*i)["host"].asString());
		uint32_t serv_id = getService(host_id, i.key().asString());
		uint32_t failed  = 0;
		uint32_t ok      = 0;
		hosts.insert(host_id);

		// adding process
		for (Json::Value::iterator j = (*i)["process"].begin();j!=(*i)["process"].end();j++) {
			//std::cout << (*j)["name"] << std::endl;
			std::string querystr;
			if ((*j)["status"].asString() == "ok") {
				ok++;
				querystr = "insert into serviceProcess(serv_id,name,full_path,cwd,username,pid,status,timestamp) values ("+std::to_string(serv_id)+
					",'"+(*j)["name"].asString()+"','"+(*j)["full_path"].asString()+
					"','"+(*j)["cwd"].asString()+"','"+(*j)["username"].asString()+
					"',"+(*j)["pid"].asString()+",'"+(*j)["status"].asString()+
					"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE full_path='"+(*j)["full_path"].asString()+
					"',cwd='"+(*j)["cwd"].asString()+"',username='"+(*j)["username"].asString()+
					"',pid="+(*j)["pid"].asString()+",status='"+(*j)["status"].asString()+"',timestamp="+std::to_string(fp_ms.count());
			} else {
				failed++;
				querystr = "insert into serviceProcess(serv_id,name,full_path,cwd,username,pid,status,timestamp) values ("+std::to_string(serv_id)+
					",'"+(*j)["name"].asString()+"','"+(*j)["full_path"].asString()+
					"','"+(*j)["cwd"].asString()+"','"+(*j)["username"].asString()+
					"',0,'"+(*j)["status"].asString()+
					"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE full_path='"+(*j)["full_path"].asString()+
					"',pid=0,status='"+(*j)["status"].asString()+"',timestamp="+std::to_string(fp_ms.count());
			}
			mysqlpp::Query q = db->query(querystr);
			myqExec(q, "Failed to insert process")
		}

		// adding sockets
		for (Json::Value::iterator j = (*i)["sockets"].begin();j!=(*i)["sockets"].end();j++) {
			//std::cout << (*j)["name"] << std::endl;
			if ((*j)["status"].asString() != "ok")
				failed++;
			else	ok++;

			mysqlpp::Query q = db->query(
				"insert into serviceSockets(serv_id,name,status,timestamp) values ("+std::to_string(serv_id)+
				",'"+(*j)["name"].asString()+"','"+(*j)["status"].asString()+
				"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE status='"+(*j)["status"].asString()+
				"',timestamp="+std::to_string(fp_ms.count()));
			myqExec(q, "Failed to insert socket")
		}
		
		// updating status
		mysqlpp::Query p = db->query(
			"insert into serviceHistory(serv_id,timestamp,failed,missing,ok) values ("+std::to_string(serv_id)+	","+std::to_string(fp_ms.count())+","+std::to_string(failed)+",0,"+std::to_string(ok)+") ON DUPLICATE KEY UPDATE failed="+std::to_string(failed)+",ok="+std::to_string(ok)
		);
		myqExec(p, "Failed to insert history")
	}

	// update history for missing service on known hosts
	for (std::set<uint32_t>::iterator i = hosts.begin();i!=hosts.end();i++) {
		mysqlpp::Query p = db->query(
			"insert into serviceHistory select s.id as serv_id, "+std::to_string(fp_ms.count())+" as timestamp, 0 as failed, ifnull(p.cnt,0)+ifnull(o.cnt,0) as missing, 0 as ok from services s left join (select serv_id, count(*) as cnt from serviceProcess group by serv_id) p on s.id=p.serv_id left join (select serv_id, count(*) as cnt from serviceSockets group by serv_id) o on s.id=o.serv_id where s.id not in (select serv_id from serviceHistory where timestamp>="+std::to_string(fp_ms.count())+") and host_id="+std::to_string(*i)
		);
		myqExec(p, "Failed to insert missing history")
	}

	mysqlpp::Connection::thread_end();
}

}
