#include "central.h"

namespace watcheD {

void	servicesClient::init() {
	//std::cout << "servicesClient::init\n";
	// Here for futur use (if any)
}

void	servicesClient::collect() {
	std::cout << "servicesClient::collect\n";

	// 1st: get the data from the client
	Json::Value data;
	std::shared_ptr<HttpClient::Response> resp;
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
	ss << resp->content.rdbuf();
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

	for (Json::Value::iterator i = data.begin();i!=data.end();i++) {
		uint32_t host_id = getHost((*i)["host"].asString());
		uint32_t serv_id = getService(host_id, i.key().asString());

		// adding process
		for (Json::Value::iterator j = (*i)["process"].begin();j!=(*i)["process"].end();j++) {
			//std::cout << (*j)["name"] << std::endl;
			std::string querystr;
			if ((*j)["status"].asString() == "ok") {
				querystr = "insert into serviceProcess(serv_id,name,full_path,cwd,username,pid,status,timestamp) values ("+std::to_string(serv_id)+
					",'"+(*j)["name"].asString()+"','"+(*j)["full_path"].asString()+
					"','"+(*j)["cwd"].asString()+"','"+(*j)["username"].asString()+
					"',"+(*j)["pid"].asString()+",'"+(*j)["status"].asString()+
					"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE full_path='"+(*j)["full_path"].asString()+
					"',cwd='"+(*j)["cwd"].asString()+"',username='"+(*j)["username"].asString()+
					"',pid="+(*j)["pid"].asString()+",status='"+(*j)["status"].asString()+"',timestamp="+std::to_string(fp_ms.count());
			} else {
				querystr = "insert into serviceProcess(serv_id,name,full_path,cwd,username,pid,status,timestamp) values ("+std::to_string(serv_id)+
					",'"+(*j)["name"].asString()+"','"+(*j)["full_path"].asString()+
					"','"+(*j)["cwd"].asString()+"','"+(*j)["username"].asString()+
					"',0,'"+(*j)["status"].asString()+
					"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE full_path='"+(*j)["full_path"].asString()+
					"',cwd='"+(*j)["cwd"].asString()+"',username='"+(*j)["username"].asString()+
					"',pid=0,status='"+(*j)["status"].asString()+"',timestamp="+std::to_string(fp_ms.count());
			}
			mysqlpp::Query q = db->query(querystr);
			try {
			if (! q.exec()) {
				std::cerr << "Failed to insert process" << std::endl;
				std::cerr << "\t\t" << q.error() << std::endl;
			}
			} catch(const mysqlpp::BadQuery& er) {
				std::cerr << "Query error: " << er.what() << std::endl;
				std::cerr << "Query: " << q.str() << std::endl;
			}
		}

		// adding sockets
		for (Json::Value::iterator j = (*i)["sockets"].begin();j!=(*i)["sockets"].end();j++) {
			//std::cout << (*j)["name"] << std::endl;
			mysqlpp::Query q = db->query(
				"insert into serviceSockets(serv_id,name,status,timestamp) values ("+std::to_string(serv_id)+
				",'"+(*j)["name"].asString()+"','"+(*j)["status"].asString()+
				"',"+std::to_string(fp_ms.count())+") ON DUPLICATE KEY UPDATE status='"+(*j)["status"].asString()+
				"',timestamp="+std::to_string(fp_ms.count()));
			try {
			if (! q.exec()) {
				std::cerr << "Failed to insert socket" << std::endl;
				std::cerr << "\t\t" << q.error() << std::endl;
			}
			} catch(const mysqlpp::BadQuery& er) {
				std::cerr << "Query error: " << er.what() << std::endl;
			}
		}
	}

	mysqlpp::Connection::thread_end();
}

}
