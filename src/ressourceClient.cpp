#include "backend.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>
#include <memory>

namespace watcheD {

void	ressourceClient::init(mysqlpp::Connection *p_db) {
	db = p_db;
	// define the base insert string
	std::string id_name = "host_id";
	std::string typed = "h$";
	if (isService) {
		typed   = "s$";
		id_name = "serv_id";
	}

	updateDefs(def);
	// load current events
	mysqlpp::Query q1 = db->query("select id, event_type, property, oper, value from "+typed+"res_events where end_time is null and %0:nid=%1:hid and res_id=%2:rid");
	q1.parse();
	q1.template_defaults["nid"] = id_name.c_str();
	q1.template_defaults["hid"] = host_id;
	q1.template_defaults["rid"] = res_id;
	try {
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
	} myqCatch(q1,"ressourceClient::init","Failed to get current events for "+table)


	// load factory
	mysqlpp::Query q2 = db->query("select event_type, property, oper, value from "+typed+"event_factory where (%0:nid=%1:hid or %0:nid is null) and (res_id=%2:rid or res_id is null) and (res_type=%3q:table or res_type is null)");
	q2.parse();
	q2.template_defaults["nid"] = id_name.c_str();
	q2.template_defaults["hid"] = host_id;
	q2.template_defaults["rid"] = res_id;
	q2.template_defaults["table"] = table.c_str();
	try {
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
	} myqCatch(q2,"ressourceClient::init","Failed to get event factory for "+table)
}

void ressourceClient::updateDefs(Json::Value *p_def) {
	def = p_def;
	std::string id_name = "host_id";
	std::stringstream insert;
	if (isService)
		id_name = "serv_id";
	insert << "insert into d$" << table << "(" << id_name << ", res_id";
	for (Json::Value::iterator j = def->begin();j!=def->end();j++) {
		insert << ", " << j.key().asString();
	}
	insert << ") values (" << host_id << ", " << res_id;
	int k=0;
	for (Json::Value::iterator j = def->begin();j!=def->end();j++,k++)
		insert << ", %" << k << ":" << j.key().asString();
	insert << ") ON DUPLICATE KEY UPDATE ";
	bool first=true;k=0;
	for (Json::Value::iterator j = def->begin();j!=def->end();j++,k++) {
		if(j.key().asString()=="timestamp") continue;
		if(!first)
			insert << ", ";
		first=false;
		insert << j.key().asString() << "=%" << k << ":" << j.key().asString();
	}
	
	baseInsert = insert.str();
}

void	ressourceClient::parse(mysqlpp::Connection *p_db, Json::Value *p_data) {
	db = p_db;
	std::string id_name = "host_id";
	std::string typed = "h$";
	if (isService) {
		typed   = "s$";
		id_name = "serv_id";
	}
	for (const Json::Value& line : *p_data) {
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
						query << "update "+typed+"res_events set current_value=%0:cv where id=%1:id";
						query.parse();
						query.template_defaults["cv"] = line[e->property].asFloat();
						query.template_defaults["id"] = found;
					} else {
						query << "insert into "+typed+"res_events (%0:name, res_id, start_time, event_type, property, current_value, oper, value) values (%1:id, %2:rid, %3:ts, %4:type, %5q:prop, %6:cv, %7q:op, %8:val)";
						query.parse();
						query.template_defaults["name"] = id_name.c_str();
						query.template_defaults["id"]   = host_id;
						query.template_defaults["rid"]  = res_id;
						query.template_defaults["ts"]   = line["timestamp"].asFloat();
						query.template_defaults["type"] = e->event_type;
						query.template_defaults["prop"] = e->property.c_str();
						query.template_defaults["cv"]   = line[e->property].asFloat();
						query.template_defaults["op"]   = e->oper;
						query.template_defaults["val"]  = e->value;
					}
					myqExec(query, "ressourceClient::collect", "Update an event for "+table+" ("+std::to_string(host_id)+","+std::to_string(res_id)+")")

					if (found == 0) {
						current_events[query.insert_id()] = std::make_shared<res_event>(*e);
						if (typed == "s$")
							alert->sendServRessource(host_id, res_id, current_events[query.insert_id()], line[e->property].asDouble());
						else
							alert->sendHostRessource(host_id, res_id, current_events[query.insert_id()], line[e->property].asDouble());
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
					mysqlpp::Query query = db->query("update "+typed+"res_events set end_time=%0:end where id=%1:id");
					query.parse();
					query.template_defaults["name"] = line["timestamp"].asFloat();
					query.template_defaults["id"]   = id;
					myqExec(query, "ressourceClient::collect", "Update an event for "+table+" ("+std::to_string(host_id)+","+std::to_string(res_id)+")")
					current_events.erase(id);
				}
			}
		}

		// Load that data into database
		mysqlpp::Query insertQuery = db->query();
		insertQuery << baseInsert;
		insertQuery.parse();
		// insert the value
		for (Json::Value::iterator j = def->begin();j!=def->end();j++) {
			std::string k = j.key().asString();
			insertQuery.template_defaults[k.c_str()] = line[k.c_str()].asFloat();
		}
		myqExec(insertQuery, "ressourceClient::collect", "Insert a value in "+table+" ("+std::to_string(host_id)+","+std::to_string(res_id)+"): "+baseInsert)
	}
}

}
