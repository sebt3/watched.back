#pragma once

#include <json/json.h>

#include <map>
#include <string>
#include <thread>


#include "server_http.hpp"
#include "client_http.hpp"
typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;


namespace watcheD {
/*********************************
 * Config
 */
class Config {
public:
	Config(std::string p_fname);
	void save();
	Json::Value* 	getServer() { return &(data["server"]); }
	Json::Value* 	getDB();
	Json::Value* 	getCentral();
	Json::Value* 	getAggregate();

private:
	Json::Value	data;
	std::string	fname;
};
}
