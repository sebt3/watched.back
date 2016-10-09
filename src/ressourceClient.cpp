#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

using namespace watcheD;

void	ressourceClient::init() {
	// define the base insert string
	std::stringstream insert;
	insert << "insert into " << table << "(agent_id, res_id";
	for (Json::Value::iterator j = def->begin();j!=def->end();j++) {
		insert << ", " << j.key().asString();
	}
	insert << ") values (" << agt_id << ", " << res_id;
	baseInsert = insert.str();

	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	// load current events
	mysqlpp::Query q1 = db->query();
	q1 << "select id, event_type, property, oper, value from events where end_time is null and agent_id=" << agt_id << " and res_id=" << res_id;
	if (mysqlpp::StoreQueryResult r1 = q1.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator i1= r1.begin(); i1 != r1.end(); ++i1) {
			mysqlpp::Row row	= *i1;
			struct event *e 	= new event;
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
	q2 << " where (agent_id =" << agt_id << " or agent_id is null)";
	q2 << "   and (res_id=" << res_id << " or res_id is null)";
	q2 << "   and (res_type='" << table << "' or res_type is null)";
	if (mysqlpp::StoreQueryResult r2 = q2.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator i2= r2.begin(); i2 != r2.end(); ++i2) {
			mysqlpp::Row row = *i2;
			struct event *e = new event;
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
	query << "select max(timestamp) from "+table+" where agent_id=" << agt_id << " and res_id=" << res_id;
	if (mysqlpp::StoreQueryResult res = query.store()) {
		mysqlpp::Row row = *res.begin(); // there should be only one row anyway
		if (row[0]!=mysqlpp::null){
			//std::cout << row[0] << std::endl;
			mysqlpp::Connection::thread_end();
			return double(row[0]);
		}
	}
	mysqlpp::Connection::thread_end();
	return -1;
}

void	ressourceClient::collect() {
	std::string url = baseurl;
	double since = getSince();
	if (since >0)
		url += "\?since="+std::to_string(since);

	Json::Value data;
	std::shared_ptr<HttpClient::Response> resp;
	std::stringstream ss;
	try {
		resp = client->request("GET", url);
	} catch (std::exception &e) {
		std::cerr << "Failed to get :"+url+"\n";
		std::cerr << e.what() << std::endl;
		return;
	}
	ss << resp->content.rdbuf();
	try {
		ss >> data;
	} catch(const Json::RuntimeError er) {
		std::cerr << "Json parse failed for url : " << url << "\n" ;
		return;
	}
	
	//std::cout << "Collecting " << agt_id << "," << res_id << "(" << current_events.size() << ")\n";

	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
	for (const Json::Value& line : data) {
		// compare values to the factory
		for (std::vector<struct event*>::iterator it = event_factory.begin() ; it != event_factory.end(); ++it) {
			struct event *e = *it;
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
					for(std::map<uint32_t, struct event*>::iterator i = current_events.begin();i != current_events.end();i++) {
						struct event *f = i->second;
						if (*e == *f)
							found  = i->first;
					}
					mysqlpp::Query query = db->query();
					if (found>0) {
						// if the event already exist update
						query << "update events set current_value=" << line[e->property] << " where id=" << found;
					} else {
						query << "insert into events (agent_id, res_id, start_time, event_type, property, current_value, oper, value) values (" << agt_id << ", " << res_id << ", " << line["timestamp"] << ", " << e->event_type << ", '" << e->property << "', " <<  line[e->property] << ", '" << e->oper << "', " << e->value << ")";
					}
					try {
					if (! query.exec()) {
						std::cerr << "Failed to " << query << std::endl;
						std::cerr << "\t\t" << query.error() << std::endl;
					}
					} catch(const mysqlpp::BadQuery& er) {
						std::cerr << "Query error: " << er.what() << std::endl;
					}

					if (found == 0) {
						current_events[query.insert_id()] = new event(*e);
						std::cout << "adding the event to the known event " << query.insert_id() << " -> " << current_events[query.insert_id()]->value << "\n";
					}
				}
			}
		}

		// check for existing events done
		for(std::map<uint32_t, struct event*>::iterator i = current_events.begin();i != current_events.end();i++) {
			struct event *e = i->second;
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
					//std::cout << "triggering an end event\n";
					mysqlpp::Query query = db->query();
					query << "update events set end_time=" << line["timestamp"] << " where id=" << id;
					try {
					if (! query.exec()) {
						std::cerr << "Failed to " << query << std::endl;
						std::cerr << "\t\t" << query.error() << std::endl;
					}
					} catch(const mysqlpp::BadQuery& er) {
						std::cerr << "Query error: " << er.what() << std::endl;
					}
					delete current_events[id];
					current_events.erase(id);
				} //else { std::cout << line[e->property].asDouble() << " - " << e->value << "\n"; }
			} else { std::cout << "Woops\n"; }
		}

		// insert the value
		mysqlpp::Query query = db->query();
		query << baseInsert;
		for (Json::Value::iterator j = def->begin();j!=def->end();j++) {
			query << ", " << line[j.key().asString()];
		}
		query << ")";
		try {
		if (! query.exec()) {
			std::cerr << "Failed to " << query << std::endl;
			std::cerr << "\t\t" << query.error() << std::endl;
		}
		} catch(const mysqlpp::BadQuery& er) {
			std::cerr << "Query error: " << er.what() << std::endl;
		}
	}
	mysqlpp::Connection::thread_end();
}
