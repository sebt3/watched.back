#include "central.h"
#include "config.h"

#include <list>
#include <fstream>

using namespace std;
namespace watcheD {

HttpClient::HttpClient(std::string p_baseurl, bool p_use_ssl, Json::Value* p_cfg): use_ssl(false), base_url(p_baseurl), cfg(p_cfg) {
	struct stat buffer;
	std::string sslkey	= (*cfg)["SSL_key"].asString();
	std::string sslcert	= (*cfg)["SSL_cert"].asString();
	std::string sslvrf	= (*cfg)["SSL_verify"].asString();
	use_ssl = (	(stat (sslcert.c_str(), &buffer) == 0) && // check if files exist
			(stat (sslvrf.c_str(), &buffer) == 0) && 
			(stat (sslkey.c_str(), &buffer) == 0) );
	if (p_use_ssl && !use_ssl)
		std::cerr << "Requiered certificate files not found, trying http instead\n";
	else if (!p_use_ssl)
		use_ssl= false;
	if (use_ssl) {
		https = std::make_shared<SWHttpsClient>(base_url,true,sslcert,sslkey,sslvrf); 
	} else {
		http = std::make_shared<SWHttpClient>(base_url);
	}
}

std::string HttpClient::request(std::string p_opt, std::string p_path) {
	std::stringstream ss;
	if (use_ssl) {
		std::shared_ptr<SWHttpsClient::Response> resp;
		resp = https->request(p_opt, p_path);
		ss << resp->content.rdbuf();
	} else {
		std::shared_ptr<SWHttpClient::Response> resp;
		resp = http->request(p_opt, p_path);
		ss << resp->content.rdbuf();
	}
	return ss.str();
}

Config::Config(std::string p_fname) : fname(p_fname) {
	// reading the file
	ifstream cfgif (fname);
	if (cfgif.good()) {
		cfgif >> data;
		cfgif.close();
	}

	// Server configuration
	//if (! data.isMember("server") || ! data["server"].isMember("thread")) {
	//	data["server"]["threads"] = 8;
	//	data["server"]["threads"].setComment(std::string("/*\t\tNumber of concurrent threads use to reply on network interface */"), Json::commentAfterOnSameLine);
	//}
	//if (! data["server"].isMember("host")) {
	//	data["server"]["host"] = "";
	//	data["server"]["host"].setComment(std::string("/*\t\tHost string to listen on (default: all interfaces) */"), Json::commentAfterOnSameLine);
	//}
	//if (! data["server"].isMember("port")) {
	//	data["server"]["port"] = 9080;
	//	data["server"]["port"].setComment(std::string("/*\t\tTCP port number */"), Json::commentAfterOnSameLine);
	//}
	//if (! data["server"].isMember("collectors_dir")) {
	//	data["server"]["collectors_dir"] = "plugins";
	//}
	// Backend configuration
	if (! data.isMember("backend") || ! data["backend"].isMember("SSL_cert")) {
		data["backend"]["SSL_cert"] = WATCHED_SSL_CERT;
		data["backend"]["SSL_cert"].setComment(std::string("/*\t\tSSL certificate file for the backend */"), Json::commentAfterOnSameLine);
	}
	if (! data["backend"].isMember("SSL_key")) {
		data["backend"]["SSL_key"] = WATCHED_SSL_KEY;
		data["backend"]["SSL_key"].setComment(std::string("/*\t\tSSL backend private key file */"), Json::commentAfterOnSameLine);
	}
	if (! data["backend"].isMember("SSL_verify")) {
		data["backend"]["SSL_verify"] = WATCHED_SSL_VRF;
		data["backend"]["SSL_verify"].setComment(std::string("/*\t\tSSL certificate file containing the agents keychain */"), Json::commentAfterOnSameLine);
	}
}

void 		Config::save() {
	std::ofstream cfgof (fname, std::ifstream::out);
	cfgof<<data;
	cfgof.close();
}

Json::Value* 	Config::getAggregate() { 
	Json::Value obj_value(Json::objectValue);
	if(! data.isMember("aggregate")) {
		data["aggregate"] = obj_value;
		data["aggregate"].setComment(std::string("/*\tConfigure the data aggregate process */"), Json::commentBefore);
	}
	return &(data["aggregate"]); 
}

Json::Value* 	Config::getCentral() {
	Json::Value obj_value(Json::objectValue);
	if(! data.isMember("central")) {
		data["central"] = obj_value;
		data["central"].setComment(std::string("/*\tCentral server specific configuration */"), Json::commentBefore);
	}
	return &(data["central"]);
}

Json::Value* 	Config::getDB() {
	Json::Value obj_value(Json::objectValue);
	if(! data.isMember("db")) {
		data["db"] = obj_value;
		data["db"].setComment(std::string("/*\tMySQL database informations */"), Json::commentBefore);
	}
	if (!data["db"].isMember("connection_string")) {
		data["db"]["connection_string"]	= "localhost:3306";
		data["db"]["connection_string"].setComment(std::string("/*\tMySQL database connection string */"), Json::commentAfterOnSameLine);
	}
	if (!data["db"].isMember("database_name")) {
		data["db"]["database_name"]		= "sebtest";
		data["db"]["database_name"].setComment(std::string("/*\t\tMySQL database name */"), Json::commentAfterOnSameLine);
	}
	if (!data["db"].isMember("login")) {
		data["db"]["login"]			= "seb";
		data["db"]["login"].setComment(std::string("/*\t\t\tMySQL login */"), Json::commentAfterOnSameLine);
	}
	if (!data["db"].isMember("password")) {
		data["db"]["password"]		= "seb";
		data["db"]["password"].setComment(std::string("/*\t\t\tMySQL password */"), Json::commentAfterOnSameLine);
	}
	if (!data["db"].isMember("pool_size")) {
		data["db"]["pool_size"]		= 32;
		data["db"]["pool_size"].setComment(std::string("/*\t\t\tNumber of concurrent connections to the database */"), Json::commentAfterOnSameLine);
	}

	return &(data["db"]);
}

}
