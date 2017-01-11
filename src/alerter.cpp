#include "central.h"
#include "config.h"

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>

namespace watcheD {
	
std::map<std::string, alerter_maker_t* > alerterFactory;

/*********************************
 * alerter
 */

alerter::alerter(std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l): dbTools(p_db, p_l) { }
alerter::~alerter() {}

/*********************************
 * luaAlerter
 */

luaAlerter::luaAlerter(std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, std::string p_filename): alerter(p_db, p_l), have_state(true) {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	state.Load(p_filename);
	std::string cmd="ok="+std::to_string(alerter::ok)+";notice="+std::to_string(alerter::notice)+";info="+std::to_string(alerter::info)+";warning="+std::to_string(alerter::warning)+";error="+std::to_string(alerter::error)+";critical="+std::to_string(alerter::critical);
	state(cmd.c_str());
}

luaAlerter::~luaAlerter() {
	std::unique_lock<std::mutex> locker(lua); // Lua isnt exactly thread safe
	have_state = false;
}

void	luaAlerter::sendAlert(alerter::levels p_lvl, const std::string p_message) {
	std::unique_lock<std::mutex> locker(lua);  // Lua isnt exactly thread safe
	if (have_state) 
		state["sendAlert"]((int)p_lvl, p_message);
}

/*********************************
 * alerterManager
 */

alerterManager::alerterManager(std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l, Json::Value* p_cfg): dbpool(p_db),l(p_l),cfg(p_cfg) {
	if (!cfg->isMember("alerter_cpp"))
		(*cfg)["alerter_cpp"] = WATCHED_DLL_ALERT;
	if (!cfg->isMember("alerter_lua"))
		(*cfg)["alerter_lua"] = WATCHED_LUA_ALERT;

	DIR *			dir;
	struct dirent*		ent;
	struct stat 		st;
	void*			dlib;
	std::string		dirname = (*cfg)["alerter_cpp"].asString();
	dir = opendir(dirname.c_str());
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			const std::string file_name = ent->d_name;
			const std::string full_file_name = dirname + "/" + file_name;

			if (file_name[0] == '.')
				continue;
			if (stat(full_file_name.c_str(), &st) == -1)
				continue;
			const bool is_directory = (st.st_mode & S_IFDIR) != 0;
			if (is_directory)
				continue;


			if (file_name.substr(file_name.rfind(".")) == ".so") {
				dlib = dlopen(full_file_name.c_str(), RTLD_NOW);
				if(dlib == NULL){
					l->error("alerterManager::", std::string(dlerror())+" while loading "+full_file_name); 
					//exit(-1);
				}
			}
		}
		closedir(dir);
	} else	l->warning("alerterManager::", dirname+" doesnt exist. No alerter plugins will be used");
	// instanciate the C++ alerters
	for(std::map<std::string, alerter_maker_t* >::iterator factit = alerterFactory.begin();factit != alerterFactory.end(); factit++)
		alerters.push_back(factit->second(l));

	dirname = (*cfg)["alerter_lua"].asString();
	dir = opendir(dirname.c_str());
	if (dir != NULL) {
		while ((ent = readdir(dir)) != NULL) {
			const std::string file_name = ent->d_name;
			const std::string name = file_name.substr(0,file_name.rfind("."));
			const std::string full_file_name = dirname + "/" + file_name;

			if (file_name[0] == '.')
				continue;
			if (stat(full_file_name.c_str(), &st) == -1)
				continue;
			const bool is_directory = (st.st_mode & S_IFDIR) != 0;
			if (is_directory)
				continue;

			if (file_name.substr(file_name.rfind(".")) == ".lua")
				alerters.push_back(std::make_shared<luaAlerter>(dbpool,l, full_file_name));
		}
		closedir(dir);
	} else	l->warning("alerterManager::", dirname+" doesnt exist. No lua alerter plugins will be used");
}

void	alerterManager::sendService(uint32_t p_host_id, uint32_t p_serv_id, std::string p_msg) {
	send(alerter::critical, p_msg);
}

void	alerterManager::sendLog(uint32_t p_host_id, uint32_t p_serv_id, uint32_t p_level, std::string p_lines) {
	// TODO: build a better message
	send((alerter::levels)p_level, p_lines);
}

void	alerterManager::sendServRessource(uint32_t p_serv_id, std::shared_ptr<res_event> p_event, double p_current) {
	// TODO: build a better message
	std::string t="Service";
	send((alerter::levels)p_event->event_type, t);
}

void	alerterManager::sendHostRessource(uint32_t p_host_id, std::shared_ptr<res_event> p_event, double p_current) {
	// TODO: build a better message
	std::string t="Host";
	send((alerter::levels)p_event->event_type, t);
}

void	alerterManager::send(alerter::levels p_lvl,  const std::string p_message) {
	bool found=false;
	for (std::vector< std::shared_ptr<alerter> >::iterator i=alerters.begin();i!=alerters.end();i++) {
		found=true;
		(*i)->sendAlert(p_lvl, p_message);
	}
	if(!found)
		l->warning("alerterManager::send", "No alerter to handle alert: "+p_message);
}

}
