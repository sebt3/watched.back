#include "backend.h"
#include "config.h"
#include <signal.h>

using namespace watcheD;

const std::string SERVER_HEAD="watched.back/0.1";
const std::string APPS_NAME="watched.back";
const std::string APPS_DESC="Watch over wasted being washed up";

void sig_handler(int num) {
	if(num == SIGUSR1) {
		std::cout << "SIGUSR1 received\n";
	}
}

int main(int argc, char *argv[]) {
	signal(SIGUSR1, sig_handler);
	std::string cfgfile			= WATCHED_CONFIG;
	if (argc>1) cfgfile			= argv[1];
	std::shared_ptr<Config>		cfg	= std::make_shared<Config>(cfgfile);
	Json::Value*			bcfg	= cfg->getBackend();
	std::shared_ptr<dbPool>		db	= std::make_shared<dbPool>(cfg->getDB());
	std::shared_ptr<watcheD::log>	l	= std::make_shared<watcheD::log>(cfg->getLog());
	std::shared_ptr<alerterManager> alert	= std::make_shared<alerterManager>(db, l, cfg->getAlerter());
	std::shared_ptr<agentManager>	am	= std::make_shared<agentManager>(db, l, alert, bcfg);
	cfg->save();
	am->init(cfg->getAggregate(), argv[0], cfgfile);
	cfg->save();
	am->startThreads();
	
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds((*bcfg)["pool_freq"].asInt()));
		am->updateAgents();
		am->startThreads(); // starting missing agents
	}
}
