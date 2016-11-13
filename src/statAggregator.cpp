#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

statAggregator::statAggregator(std::shared_ptr<dbPool>	p_db, Json::Value* p_aggregCfg) : dbTools(p_db), active(false), cfg(p_aggregCfg) {
	if(! cfg->isMember("m_delay")) {
		(*cfg)["m_delay"] = 30;		// 30mns
		(*cfg)["m_delay"].setComment(std::string("/*\t\tDelay before the data is aggregated in minutes */"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("h_delay")) {
		(*cfg)["h_delay"] = 2;		// 2h
		(*cfg)["h_delay"].setComment(std::string("/*\t\tDelay before the data is aggregated in hours */"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("retention")) {
		(*cfg)["retention"] = 30;	// 1 mois
		(*cfg)["retention"].setComment(std::string("/*\t\tNumber of days the raw data is being kept (default: 1 month) */"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("am_retention")) {
		(*cfg)["am_retention"] = 120;	// 4 mois
		(*cfg)["am_retention"].setComment(std::string("/*\tNumber of days the minuts aggregates are kept (default: 4 months)*/"), Json::commentAfterOnSameLine);
	}
	if(! cfg->isMember("ah_retention")) {
		(*cfg)["ah_retention"] = 1826;	// 5 ans
		(*cfg)["ah_retention"].setComment(std::string("/*\tNumber of days the hours aggregates are kept (default: 5 years) */"), Json::commentAfterOnSameLine);
	}
}

void	statAggregator::init(){
	int m_delay = (*cfg)["m_delay"].asInt();
	int h_delay = (*cfg)["h_delay"].asInt();
	mysqlpp::Connection::thread_start();
	mysqlpp::ScopedConnection db(*dbp, true);
	if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }

	mysqlpp::Query query = db->query("select table_name from live_tables");
	if (mysqlpp::StoreQueryResult res = query.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator it= res.begin(); it != res.end(); ++it) {
			std::string tbl = (*it)[0].c_str();
			std::string colm;
			std::string colh;
			bool haveAM		= haveTable("am$"+tbl);
			bool haveAH		= haveTable("ah$"+tbl);
			std::string create_am	= "create table am$"+tbl+" as select host_id, res_id, floor(timestamp/60000)*60000.0000 as timestamp ";
			std::string create_ah	= "create table ah$"+tbl+" as select host_id, res_id, floor(timestamp/3600000)*3600000.0000 as timestamp ";
			base_am[tbl]		= "insert into am$"+tbl+" select d.host_id, d.res_id, floor(d.timestamp/60000)*60000.0000 as timestamp ";
			base_ah[tbl]		= "insert into ah$"+tbl+" select d.host_id, d.res_id, floor(d.timestamp/3600000)*3600000.0000 as timestamp ";
			
			mysqlpp::Query qcols = db->query();
			
			qcols 	<< "select col.column_name from information_schema.columns col where col.TABLE_SCHEMA=DATABASE() and col.table_name = '" << tbl 
				<< "' and col.data_type in ('int', 'double') and column_name not in ('host_id', 'res_id', 'timestamp')";
			if (mysqlpp::StoreQueryResult resc = qcols.store()) {
				for (mysqlpp::StoreQueryResult::const_iterator itc= resc.begin(); itc != resc.end(); ++itc) {
					std::string col = (*itc)[0].c_str();
					colm = ", avg("+col+") as avg_"+col+", min("+col+") as min_"+col+", max("+col+") as max_"+col+" ";
					base_am[tbl]+=colm;
					if (!haveAM)
						create_am += colm;
					else if (!tableHasColumn("am$"+tbl,"avg_"+col)) {
						// TODO: add the missing avg,min,max columns
					}
					colh  = ", avg(avg_"+col+") as avg_"+col+", min(min_"+col+") as min_"+col+", max(max_"+col+") as max_"+col+" ";
					base_ah[tbl]+=colh;
					if (!haveAH)
						create_ah += colh;
					else if (!tableHasColumn("ah$"+tbl,"avg_"+col)) {
						// TODO: add the missing avg,min,max columns
					}
				}
				base_am[tbl]+= "from "+tbl+" d, (select u.host_id, u.res_id, max(u.timestamp) as timestamp from (select x.host_id, x.res_id, max(x.timestamp) as timestamp from am$"+tbl+" x group by x.host_id, x.res_id union all select z.host_id, z.res_id, 0.0000 as timestamp from "+tbl+" z group by z.host_id, z.res_id) u group by u.host_id, u.res_id) y where floor(d.timestamp/60000)<floor(UNIX_TIMESTAMP(now())/60)-"+std::to_string(m_delay)+" and floor(d.timestamp/60000)*60000>y.timestamp and d.host_id=y.host_id and d.res_id=y.res_id group by d.host_id,d.res_id, floor(d.timestamp/60000)*60000.0000";
				base_ah[tbl]+= "from am$"+tbl+" d, (select u.host_id, u.res_id, max(u.timestamp) as timestamp from (select x.host_id, x.res_id, max(x.timestamp) as timestamp from ah$"+tbl+" x group by x.host_id, x.res_id union all select z.host_id, z.res_id, 0.0000 as timestamp from am$"+tbl+" z group by z.host_id, z.res_id) u group by u.host_id, u.res_id) y where floor(d.timestamp/3600000)<floor(UNIX_TIMESTAMP(now())/3600)-"+std::to_string(h_delay)+" and floor(d.timestamp/3600000)*3600000>y.timestamp and d.host_id=y.host_id and d.res_id=y.res_id group by d.host_id,d.res_id, floor(d.timestamp/3600000)*3600000.0000";
				
				if(!haveAM) {
					create_am += "from "+tbl+" where floor(timestamp/60000)<floor(UNIX_TIMESTAMP(now())/60)-"+std::to_string(m_delay)+" group by host_id,res_id, floor(timestamp/60000)*60000.0000";
					mysqlpp::Query qam = db->query(create_am);
					try {
					if (! qam.execute()) {
						std::cerr << "Failed to " << create_am << std::endl;
					}
					} catch(const mysqlpp::BadQuery& er) {
						std::cerr << "Query error: " << er.what() << std::endl;
						std::cerr << "Query: " << create_am << std::endl;
					}

					mysqlpp::Query qama = db->query("alter table am$"+tbl+" add constraint primary key (host_id, res_id, timestamp)");
					if (! qama.execute()) {
						std::cerr << "Failed to alter table am$"+tbl+" add constraint primary key (host_id, res_id, timestamp)" << std::endl;
					}
				}
				if(!haveAH) {
					create_ah += "from am$"+tbl+" where floor(timestamp/3600000)<floor(UNIX_TIMESTAMP(now())/3600)-"+std::to_string(h_delay)+" group by host_id,res_id, floor(timestamp/3600000)*3600000.0000";

					mysqlpp::Query qah = db->query(create_ah);
					if (! qah.execute()) {
						std::cerr << "Failed to " << create_ah << std::endl;
					}
					mysqlpp::Query qaha = db->query("alter table ah$"+tbl+" add constraint primary key (host_id, res_id, timestamp)");
					if (! qaha.execute()) {
						std::cerr << "Failed to alter table ah$"+tbl+" add constraint primary key (host_id, res_id, timestamp)" << std::endl;
					}
				}
			}

		}
	}

	mysqlpp::Connection::thread_end();
}

void	statAggregator::startThread() {
		if (active) return;
		active=true;
		my_thread = std::thread ([this](){
		const int sec2day = 60*60*24;
		std::string q;
		while (active) {
			mysqlpp::Connection::thread_start();
			mysqlpp::ScopedConnection db(*dbp, true);
			if (!db) { std::cerr << "Failed to get a connection from the pool!" << std::endl; return; }
			//std::cout << "===================================================================================" << std::endl;
			for(std::map<std::string,std::string>::iterator i = base_am.begin();i != base_am.end();i++) {
				//std::cout << "Aggregating "+i->first << std::endl;
				mysqlpp::Query query = db->query();
				query << base_am[i->first];
				try {
					query.execute();
				} catch(const mysqlpp::BadQuery& er) {
					std::cerr << "===================================================================================" << std::endl;
					std::cerr << base_am[i->first] << std::endl;
					std::cerr << "Query error: " << er.what() << std::endl;
				}
				query << base_ah[i->first];
				try {
					query.execute();
				} catch(const mysqlpp::BadQuery& er) {
					std::cerr << "===================================================================================" << std::endl;
					std::cerr << base_ah[i->first] << std::endl;
					std::cerr << "Query error: " << er.what() << std::endl;
				}
				q="delete from "+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+(*cfg)["retention"].asString()+")*1000*"+std::to_string(sec2day);
				query <<q;
				try {
					query.execute();
				} catch(const mysqlpp::BadQuery& er) {
					std::cerr << "===================================================================================" << std::endl;
					std::cerr << q << std::endl;
					std::cerr << "Query error: " << er.what() << std::endl;
				}
				q="delete from am$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+(*cfg)["am_retention"].asString()+")*1000*"+std::to_string(sec2day);
				query <<q;
				try {
					query.execute();
				} catch(const mysqlpp::BadQuery& er) {
					std::cerr << "===================================================================================" << std::endl;
					std::cerr << q << std::endl;
					std::cerr << "Query error: " << er.what() << std::endl;
				}
				q="delete from ah$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+(*cfg)["ah_retention"].asString()+")*1000*"+std::to_string(sec2day);
				query <<q;
				try {
					query.execute();
				} catch(const mysqlpp::BadQuery& er) {
					std::cerr << "===================================================================================" << std::endl;
					std::cerr << q << std::endl;
					std::cerr << "Query error: " << er.what() << std::endl;
				}
			}
			mysqlpp::Connection::thread_end();
			std::this_thread::sleep_for(std::chrono::seconds((*cfg)["m_delay"].asInt()*60));
		}
	});
}

}
