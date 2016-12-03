#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>
#include <memory>

namespace watcheD {

void	ressourceClient::init() {
	// define the base insert string
	std::stringstream insert;
	insert << "insert into " << table << "(host_id, res_id";
	for (Json::Value::iterator j = def->begin();j!=def->end();j++) {
		insert << ", " << j.key().asString();
	}
	insert << ") values (" << host_id << ", " << res_id;
	baseInsert = insert.str();

	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	// load current events
	mysqlpp::Query q1 = db->query();
	q1 << "select id, event_type, property, oper, value from res_events where end_time is null and host_id=" << host_id << " and res_id=" << res_id;
	if (mysqlpp::StoreQueryResult r1 = q1.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator i1= r1.begin(); i1 != r1.end(); ++i1) {
			mysqlpp::Row row	= *i1;
			std::shared_ptr<struct res_event> e 	= std::make_shared<struct res_event>();
			e->event_type		= row[1];
			e->property		= row[2].c_str();
			e->oper			= row[3].c_str()[0];
			e->value		= row[4];
			current_events[row[0]]	= e;
		}
	}

	// load factory
	mysqlpp::Query q2 = db->query();
	q2 << "select event_type, property, oper, value from event_factory";
	q2 << " where (host_id =" << host_id << " or host_id is null)";
	q2 << "   and (res_id=" << res_id << " or res_id is null)";
	q2 << "   and (res_type='" << table << "' or res_type is null)";
	if (mysqlpp::StoreQueryResult r2 = q2.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator i2= r2.begin(); i2 != r2.end(); ++i2) {
			mysqlpp::Row row = *i2;
			std::shared_ptr<struct res_event> e = std::make_shared<res_event>();
			e->event_type	= row[0];
			e->property	= row[1].c_str();
			e->oper		= row[2].c_str()[0];
			e->value	= row[3];
			event_factory.push_back(e);
		}
	}
	mysqlpp::Connection::thread_end();
}

double  ressourceClient::getSince() {
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return -1; }
	mysqlpp::Query query = db->query();
	query << "select max(timestamp) from "+table+" where host_id=" << host_id << " and res_id=" << res_id;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		if (row[0]!=mysqlpp::null) {
			mysqlpp::Connection::thread_end();
			return double(row[0]);
		}
	}
	mysqlpp::Connection::thread_end();
	return -1;
}

void	ressourceClient::collect() {
	std::string url = baseurl;
	// Get the last timestamp collected
	double since = getSince();
	if (since >0)
		url += "\?since="+std::to_string(since);

	// Get the lastest data from the agent
	Json::Value data;
	std::string resp;
	std::stringstream ss;
	try {
		resp = client->request("GET", url);
	} catch (std::exception &e) {
		try {
			resp = client->request("GET", url);
		} catch (std::exception &e) {
			std::cerr << "Failed to get "+url+" after a retry:" << e.what() << std::endl;
			return;
		}
	}
	ss << resp;
	try {
		ss >> data;
	} catch(const Json::RuntimeError &er) {
		std::cerr << "Json parse failed for url : " << url << "\n" ;
		return;
	}

	// Load that data into database
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	for (const Json::Value& line : data) {
		// compare values to the factory
		for (std::vector< std::shared_ptr<struct res_event> >::iterator it = event_factory.begin() ; it != event_factory.end(); ++it) {
			std::shared_ptr<struct res_event> e = *it;
			if (line.isMember(e->property) && ! line[e->property].isNull() ) {
				bool trig = false;
				switch (e->oper) {
					case '<':
						trig = (line[e->property].asDouble() < e->value);
						break;
					case '>':
						trig = (line[e->property].asDouble() > e->value);
						break;
					case '=':
						trig = (line[e->property].asDouble() == e->value);
						break;
				}
				if (trig) {
					uint32_t found = 0;
					for(std::map<uint32_t, std::shared_ptr<struct res_event> >::iterator i = current_events.begin();i != current_events.end();i++) {
						std::shared_ptr<struct res_event> f = i->second;
						if (*e == *f)
							found  = i->first;
					}
					mysqlpp::Query query = db->query();
					if (found>0) {
						// if the event already exist update
						query << "update res_events set current_value=" << line[e->property] << " where id=" << found;
					} else {
						query << "insert into res_events (host_id, res_id, start_time, event_type, property, current_value, oper, value) values (" << host_id << ", " << res_id << ", " << line["timestamp"] << ", " << e->event_type << ", '" << e->property << "', " <<  line[e->property] << ", '" << e->oper << "', " << e->value << ")";
					}
					myqExec(query, "Update an event")

					if (found == 0) {
						current_events[query.insert_id()] = std::make_shared<res_event>(*e);
						/* std::cout << "adding the event to the known event " << query.insert_id() << " -> " << current_events[query.insert_id()]->value << "\n";*/
					}
				}
			}
		}

		// check for existing events done
		for(std::map<uint32_t, std::shared_ptr<struct res_event> >::iterator i = current_events.begin();i != current_events.end();i++) {
			std::shared_ptr<struct res_event> e = i->second;
			uint32_t id = i->first;
			if (line.isMember(e->property) && ! line[e->property].isNull() ) {
				bool trig = false;
				switch (e->oper) {
					case '<':
						trig = (line[e->property].asDouble() >= e->value);
						break;
					case '>':
						trig = (line[e->property].asDouble() <= e->value);
						break;
					case '=':
						trig = (line[e->property].asDouble() != e->value);
						break;
				}
				if (trig) {
					mysqlpp::Query query = db->query();
					query << "update res_events set end_time=" << line["timestamp"] << " where id=" << id;
					myqExec(query, "Update an event")
					current_events.erase(id);
				}
			}
		}

		// insert the value
		mysqlpp::Query query = db->query();
		query << baseInsert;
		for (Json::Value::iterator j = def->begin();j!=def->end();j++)
			query << ", " << line[j.key().asString()];
		query << ") ON DUPLICATE KEY UPDATE ";
		bool first=true;
		for (Json::Value::iterator j = def->begin();j!=def->end();j++) {
			if(j.key().asString()=="timestamp") continue;
			if(!first)
				query << ", ";
			first=false;
			query << j.key().asString() << "=" << line[j.key().asString()];
		}

		myqExec(query, "Insert a value")
	}
	mysqlpp::Connection::thread_end();
}

}
