#include "central.h"
#include "config.h"
using namespace watcheD;

const std::string SERVER_HEAD="watched.central/0.1";
const std::string APPS_NAME="watched.central";
const std::string APPS_DESC="Watch over wasted being washed up";

int main(int argc, char *argv[]) {
	std::shared_ptr<Config>		cfg	= std::make_shared<Config>(WATCHED_CONFIG);
	Json::Value*			dbCfg	= cfg->getDB();

	std::shared_ptr<dbPool>		db	= std::make_shared<dbPool>(dbCfg);
	std::shared_ptr<agentManager>	ac	= std::make_shared<agentManager>(db);
	cfg->save();
	ac->init(cfg->getAggregate());
	cfg->save();
	ac->startThreads();
	
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
}
