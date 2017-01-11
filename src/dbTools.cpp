#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

/*********************************
 * dbTools
 */

bool	dbTools::haveProcessStatus(uint32_t p_serv_id, std::string p_name, std::string p_status) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::haveProcessStatus", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from s$process where serv_id="<<p_serv_id<<" and name=" << mysqlpp::quote << p_name << " and status=" << mysqlpp::quote << p_status;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
		
	}
	mysqlpp::Connection::thread_end();
	return false;
}

bool	dbTools::haveSocketStatus(uint32_t p_serv_id, std::string p_name, std::string p_status) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::haveSocketStatus", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from s$sockets where serv_id="<<p_serv_id<<" and name=" << mysqlpp::quote << p_name << " and status=" << mysqlpp::quote << p_status;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
		
	}
	mysqlpp::Connection::thread_end();
	return false;
}

bool	dbTools::haveLogEvent(uint32_t p_serv_id, std::string p_source_name, uint32_t p_line_no, std::string p_date_field) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::haveLogEvent", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from s$log_events where serv_id="<<p_serv_id<<" and source_name=" << mysqlpp::quote << p_source_name << " and line_no="<<p_line_no<<" and date_field=" << mysqlpp::quote << p_date_field;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
		
	}
	mysqlpp::Connection::thread_end();
	return false;
}

bool	dbTools::haveTable(std::string p_name) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::haveTable", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from information_schema.tables where TABLE_SCHEMA=DATABASE() and table_name=" << mysqlpp::quote << p_name;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
		
	}
	mysqlpp::Connection::thread_end();
	return false;
}
bool	dbTools::haveRessource(std::string p_origin, std::string p_name) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::haveRessource", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from c$ressources where name=" << mysqlpp::quote << p_name << " and origin=" << mysqlpp::quote << p_origin;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
	}
	mysqlpp::Connection::thread_end();
	return false;
}

uint32_t dbTools::getRessourceId(std::string p_origin, std::string p_res) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::getRessourceId", "Failed to get a connection from the pool!"); return 0; }
	mysqlpp::Query query = db->query();
	query << "select id from c$ressources where name=" << mysqlpp::quote << p_res << " and origin=" << mysqlpp::quote << p_origin;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]);
		
	}
	mysqlpp::Connection::thread_end();
	return 0;
}

bool	dbTools::tableHasColumn(std::string p_name, std::string p_col) {
	if (!haveTable(p_name)) return false;
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::tableHasColumn", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from information_schema.columns where TABLE_SCHEMA=DATABASE() and table_name=" << mysqlpp::quote << p_name << mysqlpp::do_nothing << " and column_name=" << mysqlpp::quote << p_col;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
	}
	mysqlpp::Connection::thread_end();
	return false;
}

bool	dbTools::haveHost(std::string p_host_name) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::haveHost", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from h$hosts where name=" << mysqlpp::quote << p_host_name;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
	}
	mysqlpp::Connection::thread_end();
	return false;
}

uint32_t	dbTools::getHost(std::string p_host_name) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::getHost", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	if ( !haveHost(p_host_name) ) {
		mysqlpp::Query q = db->query("insert into h$hosts(name) values('"+p_host_name+"')");
		myqExec(q, "dbTools::getHost", "Failed to insert host")
	}
	query << "select id from h$hosts where name=" << mysqlpp::quote << p_host_name;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]);
		
	}
	mysqlpp::Connection::thread_end();
	return 0;
}

bool	dbTools::haveService(uint32_t p_host_id, std::string p_service) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::haveService", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from s$services where host_id="+std::to_string(p_host_id)+" and name=" << mysqlpp::quote << p_service;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
	}
	mysqlpp::Connection::thread_end();
	return false;
}

uint32_t	dbTools::getService(uint32_t p_host_id, std::string p_service) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::getService", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	if ( !haveService(p_host_id, p_service) ) {
		mysqlpp::Query q = db->query("insert into s$services(host_id,name) values("+std::to_string(p_host_id)+",'"+p_service+"')");
		myqExec(q, "dbTools::getService", "Failed to insert service")
	}
	query << "select id from s$services where host_id="+std::to_string(p_host_id)+" and name=" << mysqlpp::quote << p_service;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]);
		
	}
	mysqlpp::Connection::thread_end();
	return 0;
}

bool	dbTools::haveEventType(std::string p_event_type_name) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::haveEventType", "Failed to get a connection from the pool!"); return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from c$event_types where lower(name)=lower(" << mysqlpp::quote << p_event_type_name << ")";
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
	}
	mysqlpp::Connection::thread_end();
	return false;
}

uint32_t dbTools::getEventType(std::string p_event_type_name) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { l->error("dbTools::getEventType", "Failed to get a connection from the pool!"); return 0; }
	mysqlpp::Query query = db->query();
	if ( !haveEventType(p_event_type_name) ) {
		mysqlpp::Query q = db->query("insert into c$event_types(name) values('"+p_event_type_name+"')");
		myqExec(q, "dbTools::getEventType", "Failed to insert event_type")
	}
	query << "select id from c$event_types where lower(name)=lower(" << mysqlpp::quote << p_event_type_name << ")";
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]);
		
	}
	mysqlpp::Connection::thread_end();
	return 0;
}


}
