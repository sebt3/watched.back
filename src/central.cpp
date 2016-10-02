#include "central.h"
using namespace watcheD;

const std::string SERVER_HEAD="watched.central/0.1";
const std::string APPS_NAME="watched.central";
const std::string APPS_DESC="Watch over wasted being washed up";
#define CFG_FILE "central.config.json"

int main(int argc, char *argv[]) {
	Config cfg(CFG_FILE);
	//Json::Value*	servCfg = cfg.getServer();
	//Json::Value*	ctlCfg = cfg.getCentral();
	Json::Value*	dbCfg = cfg.getDB();
	//int port_i	= (*servCfg)["port"].asInt();
	//if (argc>1)	port_i = atoi(argv[1]);

	dbPool db(dbCfg);


	agentManager ac(&db);
	cfg.save();
	ac.init(cfg.getAggregate());
	cfg.save();
	ac.startThreads();
	
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
}
