#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

/*********************************
 * dbTools
 */

bool	dbTools::haveTable(std::string p_name) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return false; }
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
bool	dbTools::haveRessource(std::string p_name) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from ressources where name=" << mysqlpp::quote << p_name;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]) > 0;
	}
	mysqlpp::Connection::thread_end();
	return false;
}

uint32_t dbTools::getRessourceId(std::string p_res) {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return 0; }
	mysqlpp::Query query = db->query();
	query << "select id from ressources where name=" << mysqlpp::quote << p_res;
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
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return false; }
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
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from hosts where name=" << mysqlpp::quote << p_host_name;
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
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return false; }
	mysqlpp::Query query = db->query();
	if ( !haveHost(p_host_name) ) {
		mysqlpp::Query q = db->query("insert into hosts(name) values('"+p_host_name+"')");
		if (! q.execute()) {
				std::cerr << "Failed to insert hosts: " << p_host_name << std::endl;
			}
	}
	query << "select id from hosts where name=" << mysqlpp::quote << p_host_name;
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
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return false; }
	mysqlpp::Query query = db->query();
	query << "select count(*) from services where host_id="+std::to_string(p_host_id)+" and name=" << mysqlpp::quote << p_service;
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
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return false; }
	mysqlpp::Query query = db->query();
	if ( !haveService(p_host_id, p_service) ) {
		mysqlpp::Query q = db->query("insert into services(host_id,name) values("+std::to_string(p_host_id)+",'"+p_service+"')");
		if (! q.execute()) {
				std::cerr << "Failed to insert service: " << p_service << std::endl;
			}
	}
	query << "select id from services where host_id="+std::to_string(p_host_id)+" and name=" << mysqlpp::quote << p_service;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		mysqlpp::Connection::thread_end();
		return int(row[0]);
		
	}
	mysqlpp::Connection::thread_end();
	return 0;
}


}
