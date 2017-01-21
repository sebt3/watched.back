#include "central.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

statAggregator::statAggregator(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, Json::Value* p_aggregCfg) : dbTools(p_db, p_l), active(false), cfg(p_aggregCfg) {
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
	if (!db) { l->error("statAggregator::init", "Failed to get a connection from the pool!"); return; }

	mysqlpp::Query query = db->query("select table_name from c$data_tables");
	try {
	if (mysqlpp::StoreQueryResult res = query.store()) {
		for (mysqlpp::StoreQueryResult::const_iterator it= res.begin(); it != res.end(); ++it) {
			std::string tbl = (*it)[0].c_str();
			std::string id_name = "host_id";
			if (tableHasColumn("d$"+tbl, "serv_id"))
				id_name = "serv_id";
			std::string colm;
			std::string colh;
			bool haveAM		= haveTable("am$"+tbl);
			bool haveAH		= haveTable("ah$"+tbl);
			std::string create_am	= "create table am$"+tbl+" as select "+id_name+", res_id, floor(timestamp/60000)*60000.0000 as timestamp ";
			std::string create_ah	= "create table ah$"+tbl+" as select "+id_name+", res_id, floor(timestamp/3600000)*3600000.0000 as timestamp ";
			base_am[tbl]		= "insert into am$"+tbl+" select d."+id_name+", d.res_id, floor(d.timestamp/60000)*60000.0000 as timestamp ";
			base_ah[tbl]		= "insert into ah$"+tbl+" select d."+id_name+", d.res_id, floor(d.timestamp/3600000)*3600000.0000 as timestamp ";
			
			mysqlpp::Query qcols = db->query();
			
			qcols 	<< "select col.column_name from information_schema.columns col where col.TABLE_SCHEMA=DATABASE() and col.table_name = '" << tbl 
				<< "' and col.data_type in ('int', 'double') and column_name not in ('"+id_name+"', 'res_id', 'timestamp')";
			try {
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
				base_am[tbl]+= "from d$"+tbl+" d, (select u."+id_name+", u.res_id, max(u.timestamp) as timestamp from (select x."+id_name+", x.res_id, max(x.timestamp) as timestamp from am$"+tbl+" x group by x."+id_name+", x.res_id union all select z."+id_name+", z.res_id, 0.0000 as timestamp from d$"+tbl+" z group by z."+id_name+", z.res_id) u group by u."+id_name+", u.res_id) y where floor(d.timestamp/60000)<floor(UNIX_TIMESTAMP(now())/60)-"+std::to_string(m_delay)+" and floor(d.timestamp/60000)*60000>y.timestamp and d."+id_name+"=y."+id_name+" and d.res_id=y.res_id group by d."+id_name+",d.res_id, floor(d.timestamp/60000)*60000.0000";
				base_ah[tbl]+= "from am$"+tbl+" d, (select u."+id_name+", u.res_id, max(u.timestamp) as timestamp from (select x."+id_name+", x.res_id, max(x.timestamp) as timestamp from ah$"+tbl+" x group by x."+id_name+", x.res_id union all select z."+id_name+", z.res_id, 0.0000 as timestamp from am$"+tbl+" z group by z."+id_name+", z.res_id) u group by u."+id_name+", u.res_id) y where floor(d.timestamp/3600000)<floor(UNIX_TIMESTAMP(now())/3600)-"+std::to_string(h_delay)+" and floor(d.timestamp/3600000)*3600000>y.timestamp and d."+id_name+"=y."+id_name+" and d.res_id=y.res_id group by d."+id_name+",d.res_id, floor(d.timestamp/3600000)*3600000.0000";
				
				if(!haveAM) {
					create_am += "from d$"+tbl+" where floor(timestamp/60000)<floor(UNIX_TIMESTAMP(now())/60)-"+std::to_string(m_delay)+" group by "+id_name+",res_id, floor(timestamp/60000)*60000.0000";
					mysqlpp::Query qam = db->query(create_am);
					myqExec(qam, "statAggregator::init", "Failed to create aggregate AM")

					mysqlpp::Query qama = db->query("alter table am$"+tbl+" add constraint primary key ("+id_name+", res_id, timestamp)");
					myqExec(qama, "statAggregator::init", "Failed to alter aggregate AM")
					// TODO: add the foreign key to (s|h)$ressources
				}
				if(!haveAH) {
					create_ah += "from am$"+tbl+" where floor(timestamp/3600000)<floor(UNIX_TIMESTAMP(now())/3600)-"+std::to_string(h_delay)+" group by "+id_name+",res_id, floor(timestamp/3600000)*3600000.0000";

					mysqlpp::Query qah = db->query(create_ah);
					myqExec(qah, "statAggregator::init", "Failed to create aggregate AH")

					mysqlpp::Query qaha = db->query("alter table ah$"+tbl+" add constraint primary key ("+id_name+", res_id, timestamp)");
					myqExec(qaha, "statAggregator::init", "Failed to alter aggregate AH")
					// TODO: add the foreign key to (s|h)$ressources
				}
			}
			} myqCatch(qcols, "statAggregator::init","Failed to get a data_table column")
		}
	}
	} myqCatch(query, "statAggregator::init","Failed to get data_table list")

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
			if (!db) { l->error("statAggregator::thread", "Failed to get a connection from the pool!"); return; }
			//std::cout << "===================================================================================" << std::endl;
			for(std::map<std::string,std::string>::iterator i = base_am.begin();i != base_am.end();i++) {
				//std::cout << "Aggregating "+i->first << std::endl;
				mysqlpp::Query query = db->query();
				query << base_am[i->first];
				myqExec(query, "statAggregator::thread", "Failed to insert aggregate AM")
				query << base_ah[i->first];
				myqExec(query, "statAggregator::thread", "Failed to insert aggregate AH")
				q="delete from d$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+(*cfg)["retention"].asString()+")*1000*"+std::to_string(sec2day);
				query <<q;
				myqExec(query, "statAggregator::thread", "Failed to purge raw data")
				q="delete from am$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+(*cfg)["am_retention"].asString()+")*1000*"+std::to_string(sec2day);
				query <<q;
				myqExec(query, "statAggregator::thread", "Failed to purge aggregate AM")
				q="delete from ah$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+(*cfg)["ah_retention"].asString()+")*1000*"+std::to_string(sec2day);
				query <<q;
				myqExec(query, "statAggregator::thread", "Failed to purge aggregate AH")
			}
			mysqlpp::Connection::thread_end();
			std::this_thread::sleep_for(std::chrono::seconds((*cfg)["m_delay"].asInt()*60));
		}
	});
}

}
