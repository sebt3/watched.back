#include "backend.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

/*********************************
 * dbTools
 */

bool	dbTools::grab() {
	if (db!=NULL)	return true;
	//std::cout << "dbTools::grab ------ " << className <<std::endl;
	db = dbp->safe_grab();
	return db != NULL;
}

void	dbTools::release() {
	if (db==NULL)	return;
	//std::cout << "dbTools::release --- " << className <<std::endl;
	dbp->release(db);
	db = NULL;
}

bool	dbTools::haveProcessStatus(uint32_t p_serv_id, std::string p_name, std::string p_status) {
	if (!grab()) { l->error("dbTools::haveProcessStatus", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from s$process where serv_id=%0:id and name=%1q:na and status=%2q:st");
	query.parse();
	query.template_defaults["id"] = p_serv_id;
	query.template_defaults["na"] = p_name.c_str();
	query.template_defaults["st"] = p_status.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
		
	}
	} myqCatch(query, "dbTools::haveProcessStatus","Failed to get count for "+std::to_string(p_serv_id))
	return false;
}

bool	dbTools::haveSocketStatus(uint32_t p_serv_id, std::string p_name, std::string p_status) {
	if (!grab()) { l->error("dbTools::haveSocketStatus", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from s$sockets where serv_id=%0:id and name=%1q:na and status=%2q:st");
	query.parse();
	query.template_defaults["id"] = p_serv_id;
	query.template_defaults["na"] = p_name.c_str();
	query.template_defaults["st"] = p_status.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
		
	}
	} myqCatch(query, "dbTools::haveSocketStatus","Failed to get count for "+std::to_string(p_serv_id))
	return false;
}

bool	dbTools::haveLogEvent(uint32_t p_serv_id, std::string p_source_name, uint32_t p_line_no, std::string p_date_field) {
	if (!grab()) { l->error("dbTools::haveLogEvent", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from s$log_events where serv_id=%0:id and source_name=%1q:sn and line_no=%2:ln and date_field=%3q:df");
	query.parse();
	query.template_defaults["id"] = p_serv_id;
	query.template_defaults["sn"] = p_source_name.c_str();
	query.template_defaults["ln"] = p_line_no;
	query.template_defaults["df"] = p_date_field.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
		
	}
	} myqCatch(query, "dbTools::haveLogEvent","Failed to get count for "+std::to_string(p_serv_id))
	return false;
}

bool	dbTools::haveTable(std::string p_name) {
	if (!grab()) { l->error("dbTools::haveTable", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from information_schema.tables where TABLE_SCHEMA=DATABASE() and table_name=%0q:tn");
	query.parse();
	query.template_defaults["tn"] = p_name.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
		
	}
	} myqCatch(query, "dbTools::haveTable","Failed to get count for "+p_name)
	return false;
}
bool	dbTools::haveRessource(std::string p_origin, std::string p_name) {
	if (!grab()) { l->error("dbTools::haveRessource", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from c$ressources where name=%0q:tn and origin=%1q:or");
	query.parse();
	query.template_defaults["tn"] = p_name.c_str();
	query.template_defaults["or"] = p_origin.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
	}
	} myqCatch(query, "dbTools::haveRessource","Failed to get count for "+p_name)
	return false;
}

uint32_t dbTools::getRessourceId(std::string p_origin, std::string p_res) {
	if (!grab()) { l->error("dbTools::getRessourceId", "Failed to get a connection from the pool!"); return 0; }
	mysqlpp::Query query = db->query("select id from c$ressources where name=%0q:rs and origin=%1q:or");
	query.parse();
	query.template_defaults["rs"] = p_res.c_str();
	query.template_defaults["or"] = p_origin.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]);
		
	}
	} myqCatch(query, "dbTools::getRessourceId","Failed to get name for "+p_res)
	return 0;
}

std::string dbTools::getRessourceName(uint32_t p_res) {
	if (!grab()) { l->error("dbTools::getRessourceName", "Failed to get a connection from the pool!"); return 0; }
	mysqlpp::Query query = db->query("select name from c$ressources where id=%0:id");
	query.parse();
	query.template_defaults["id"] = p_res;
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return row[0].c_str();
	}
	} myqCatch(query, "dbTools::getRessourceName","Failed to get name for "+std::to_string(p_res))
	return 0;
}

bool	dbTools::tableHasColumn(std::string p_name, std::string p_col) {
	if (!haveTable(p_name)) return false;
	if (!grab()) { l->error("dbTools::tableHasColumn", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from information_schema.columns where TABLE_SCHEMA=DATABASE() and table_name=%0q:tn and column_name=%1q:cl");
	query.parse();
	query.template_defaults["tn"] = p_name.c_str();
	query.template_defaults["cl"] = p_col.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
	}
	} myqCatch(query, "dbTools::tableHasColumn","Failed to get count for "+p_name+"."+p_col)
	return false;
}

bool	dbTools::haveBackend(std::string p_me, std::string p_cfg) {
	if (!grab()) { l->error("dbTools::haveBackend", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from b$backend where hostname=%0q:h and filename=%1q:f");
	query.parse();
	query.template_defaults["h"] = p_me.c_str();
	query.template_defaults["f"] = p_cfg.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
	}
	} myqCatch(query, "dbTools::haveBackend","Failed to get count for "+p_me+" - "+p_cfg)
	return false;
}

uint32_t	dbTools::getBackend(std::string p_me, std::string p_cfg) {
	if (!grab()) { l->error("dbTools::getBackend", "Failed to get a connection from the pool!"); return false; }
	if ( !haveBackend(p_me, p_cfg) ) {
		mysqlpp::Query q = db->query("insert into b$backend(hostname,filename) values(%0q:h,%1q:f)");
		q.parse();
		q.template_defaults["h"] = p_me.c_str();
		q.template_defaults["f"] = p_cfg.c_str();
		myqExec(q, "dbTools::getBackend", "Failed to insert host")
	}
	mysqlpp::Query query = db->query("select id from b$backend where hostname=%0q:h and filename=%1q:f");
	query.parse();
	query.template_defaults["h"] = p_me.c_str();
	query.template_defaults["f"] = p_cfg.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]);
		
	}
	} myqCatch(query, "dbTools::getBackend","Failed to get id for "+p_me+" - "+p_cfg)
	return 0;
}

bool	dbTools::haveHost(std::string p_host_name) {
	if (!grab()) { l->error("dbTools::haveHost", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from h$hosts where name=%0q:h");
	query.parse();
	query.template_defaults["h"] = p_host_name.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
	}
	} myqCatch(query, "dbTools::haveHost","Failed to get count for "+p_host_name)
	return false;
}

uint32_t	dbTools::getHost(std::string p_host_name) {
	if (!grab()) { l->error("dbTools::getHost", "Failed to get a connection from the pool!"); return false; }
	if ( !haveHost(p_host_name) ) {
		mysqlpp::Query q = db->query("insert into h$hosts(name) values(%0q:h)");
		q.parse();
		q.template_defaults["h"] = p_host_name.c_str();
		myqExec(q, "dbTools::getHost", "Failed to insert host")
	}
	mysqlpp::Query query = db->query("select id from h$hosts where name=%0q:h");
	query.parse();
	query.template_defaults["h"] = p_host_name.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]);
		
	}
	} myqCatch(query, "dbTools::getHost","Failed to get id for "+p_host_name)
	return 0;
}

bool	dbTools::haveService(uint32_t p_host_id, std::string p_service) {
	if (!grab()) { l->error("dbTools::haveService", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from s$services where host_id=%0:id and name=%1q:na");
	query.parse();
	query.template_defaults["id"] = p_host_id;
	query.template_defaults["na"] = p_service.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
	}
	} myqCatch(query, "dbTools::haveService","Failed to get count for "+p_service+" on host "+std::to_string(p_host_id))

	return false;
}

uint32_t	dbTools::getService(uint32_t p_host_id, std::string p_service) {
	if (!grab()) { l->error("dbTools::getService", "Failed to get a connection from the pool!"); return false; }
	if ( !haveService(p_host_id, p_service) ) {
		mysqlpp::Query q = db->query("insert into s$services(host_id,name) values(%0:id, %1q:na)");
		q.parse();
		q.template_defaults["id"] = p_host_id;
		q.template_defaults["na"] = p_service.c_str();
		myqExec(q, "dbTools::getService", "Failed to insert service")
	}
	mysqlpp::Query query = db->query("select id from s$services where host_id=%0:id and name=%1q:na");
	query.parse();
	query.template_defaults["id"] = p_host_id;
	query.template_defaults["na"] = p_service.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]);
		
	}
	} myqCatch(query, "dbTools::getService","Failed to get id for "+p_service+" on host "+std::to_string(p_host_id))
	return 0;
}

uint32_t	dbTools::getServiceHost(uint32_t p_service) {
	if (!grab()) { l->error("dbTools::getServiceHost", "Failed to get a connection from the pool!"); return 0; }
	mysqlpp::Query query = db->query("select host_id from s$services where id=%0:id");
	query.parse();
	query.template_defaults["id"] = p_service;
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]);
	}
	} myqCatch(query, "dbTools::getServiceHost","Failed to get host_id for "+std::to_string(p_service))
	return 0;
}

std::string	dbTools::getServiceName(uint32_t p_host_id, uint32_t p_service) {
	if (!grab()) { l->error("dbTools::getServiceName", "Failed to get a connection from the pool!"); return ""; }
	mysqlpp::Query query = db->query("select name from s$services where host_id=%0:hid and id=%1:sid");
	query.parse();
	query.template_defaults["hid"] = p_host_id;
	query.template_defaults["sid"] = p_service;
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return row[0].c_str();
	}
	} myqCatch(query, "dbTools::getServiceName","Failed to get name for "+std::to_string(p_service))
	return 0;
}


std::string	dbTools::getHostName(uint32_t p_host_id) {
	if (!grab()) { l->error("dbTools::getHostName", "Failed to get a connection from the pool!"); return ""; }
	mysqlpp::Query query = db->query("select name from h$hosts where id=%0:id");
	query.parse();
	query.template_defaults["id"] = p_host_id;
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return row[0].c_str();
	}
	} myqCatch(query, "dbTools::getHostName","Failed to get name for "+std::to_string(p_host_id))
	return 0;
}

bool	dbTools::haveEventType(std::string p_event_type_name) {
	if (!grab()) { l->error("dbTools::haveEventType", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from c$event_types where lower(name)=lower(%0q:t)");
	query.parse();
	query.template_defaults["t"] = p_event_type_name.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
	}
	} myqCatch(query, "dbTools::haveEventType","Failed to get count for "+p_event_type_name)
	return false;
}

uint32_t dbTools::getEventType(std::string p_event_type_name) {
	if (!grab()) { l->error("dbTools::getEventType", "Failed to get a connection from the pool!"); return 0; }
	if ( !haveEventType(p_event_type_name) ) {
		mysqlpp::Query q = db->query("insert into c$event_types(name) values(%0q:t)");
		q.parse();
		q.template_defaults["t"] = p_event_type_name.c_str();
		myqExec(q, "dbTools::getEventType", "Failed to insert event_type")
	}
	mysqlpp::Query query = db->query("select id from c$event_types where lower(name)=lower(%0q:t)");
	query.parse();
	query.template_defaults["t"] = p_event_type_name.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]);
	}
	} myqCatch(query, "dbTools::getEventType","Failed to get id for "+p_event_type_name)
	return 0;
}

bool	dbTools::haveServiceType(std::string p_type) {
	if (!grab()) { l->error("dbTools::haveServiceType", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query("select count(*) from s$types where name=%0q:t");
	query.parse();
	query.template_defaults["t"] = p_type.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]) > 0;
	}
	} myqCatch(query, "dbTools::haveServiceType","Failed to get count for "+p_type)
	return false;
}

uint32_t dbTools::getServiceType(std::string p_type) {
	if (!grab()) { l->error("dbTools::getServiceType", "Failed to get a connection from the pool!"); return 0; }
	if ( !haveServiceType(p_type) ) {
		mysqlpp::Query q = db->query("insert into s$types(name) values(%0q:t)");
		q.parse();
		q.template_defaults["t"] = p_type.c_str();
		myqExec(q, "dbTools::getServiceType", "Failed to insert service_type")
	}
	mysqlpp::Query query = db->query("select id from s$types where name=%0q:t");
	query.parse();
	query.template_defaults["t"] = p_type.c_str();
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		return int(row[0]);
		
	}
	} myqCatch(query, "dbTools::getServiceType","Failed to get id for "+p_type)
	return 0;
}


}
