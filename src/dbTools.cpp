#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

using namespace watcheD;

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
