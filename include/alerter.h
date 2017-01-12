#pragma once
#include <memory>
#include <map>
#include <string>
#include <mutex>
#include <json/json.h>

namespace watcheD {

/*********************************
 * log
 */
class log {
public:
	log(Json::Value* p_cfg);
	void error(const std::string p_src, std::string p_message);
	void warning(const std::string p_src, std::string p_message);
	void notice(const std::string p_src, std::string p_message);
	void info(const std::string p_src, std::string p_message);
	void debug(const std::string p_src, std::string p_message);
private:
	void write(uint16_t p_lvl, const std::string p_src, std::string p_message);
	void write(std::string p_lvl, const std::string p_src, std::string p_message);

	Json::Value*	cfg;
	uint16_t	level;
	static const std::vector<std::string> levels;
	std::mutex	mutex;
};

class dbPool;
/*********************************
 * dbTools
 */
class dbTools {
public:
	dbTools(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l) : dbp(p_db), l(p_l) { }
protected:
	bool		haveTable(std::string p_name);
	bool		tableHasColumn(std::string p_name, std::string p_col);
	bool		haveRessource(std::string p_origin, std::string p_name);
	uint32_t	getRessourceId(std::string p_origin, std::string p_res);
	std::string	getRessourceName(uint32_t p_res);
	bool		haveHost(std::string p_host_name);
	uint32_t	getHost(std::string p_host_name);
	std::string	getHostName(uint32_t p_host_id);
	bool		haveEventType(std::string p_event_type_name);
	uint32_t	getEventType(std::string p_event_type_name);
	bool		haveService(uint32_t p_host_id, std::string p_service);
	uint32_t	getService(uint32_t p_host_id, std::string p_service);
	std::string	getServiceName(uint32_t p_host_id, uint32_t p_service);
	uint32_t	getServiceHost(uint32_t p_service);
	
	bool		haveLogEvent(uint32_t p_serv_id, std::string p_source_name, uint32_t p_line_no, std::string p_date_field);
	bool		haveProcessStatus(uint32_t p_serv_id, std::string p_name, std::string p_status);
	bool		haveSocketStatus(uint32_t p_serv_id, std::string p_name, std::string p_status);
	std::shared_ptr<dbPool>	dbp;
	std::shared_ptr<log>	l;
};

/*********************************
 * Alerter
 */

class alerter: public dbTools {
public:
	enum levels {ok,info,notice,warning,error,critical};
	alerter(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l);
	~alerter();
	virtual void	sendAlert(levels p_lvl, const std::string p_dest, const std::string p_title, const std::string p_message) =0;
};

/*********************************
 * Plugin management
 */
typedef std::shared_ptr<alerter> alerter_maker_t(std::shared_ptr<log> p_l);
extern std::map<std::string, alerter_maker_t* > alerterFactory;

}

#define MAKE_PLUGIN_ALERTER(className,id)			\
extern "C" {							\
std::shared_ptr<alerter> makerA_##id(std::shared_ptr<dbPool> p_db, std::shared_ptr<log> p_l){ \
   return std::make_shared<className>(p_db, p_l);		\
}								\
}								\
class proxyA_##id { public:					\
   proxyA_##id(){ alerterFactory[#id] = makerA_##id; }		\
};								\
proxyA_##id p;
