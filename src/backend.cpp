#include "backend.h"
#include "config.h"
using namespace watcheD;

const std::string SERVER_HEAD="watched.back/0.1";
const std::string APPS_NAME="watched.back";
const std::string APPS_DESC="Watch over wasted being washed up";

int main(int argc, char *argv[]) {
	std::string cfgfile			= WATCHED_CONFIG;
	if (argc>1) cfgfile			= argv[1];
	std::shared_ptr<Config>		cfg	= std::make_shared<Config>(cfgfile);
	std::shared_ptr<dbPool>		db	= std::make_shared<dbPool>(cfg->getDB());
	std::shared_ptr<watcheD::log>	l	= std::make_shared<watcheD::log>(cfg->getLog());
	std::shared_ptr<alerterManager> alert	= std::make_shared<alerterManager>(db, l, cfg->getAlerter());
	std::shared_ptr<agentManager>	ac	= std::make_shared<agentManager>(db, l, alert, cfg->getBackend());
	cfg->save();
	ac->init(cfg->getAggregate());
	cfg->save();
	ac->startThreads();
	
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(5*60)); // TODO: should configure this
		ac->updateAgents();
		ac->startThreads(); // starting missing agents
	}
}
