#include "backend.h"
#include <chrono>
#include <stdlib.h>
#include <mysql++/exceptions.h>
#include <fstream>

namespace watcheD {

statAggregator::statAggregator(std::shared_ptr<dbPool>	p_db, std::shared_ptr<log> p_l, Json::Value* p_aggregCfg) : dbTools(p_db, p_l), active(false), cfg(p_aggregCfg) {
}

void	statAggregator::init(){
	thread_freq	= 3600;
}

void	statAggregator::startThread() {
	if (active) return;
	active=true;
	my_thread = std::thread ([this](){
	const int sec2day = 60*60*24;
	while (active) {
		mysqlpp::Connection::thread_start();
		mysqlpp::ScopedConnection db(*dbp, true);
		if (!db) { l->error("statAggregator::thread", "Failed to get a connection from the pool!"); return; }

		mysqlpp::Query query = db->query("select data_type, delay_am, delay_ah, delay_ad, retention_d, retention_am, retention_ah, retention_ad from c$data_configs");
		try {
		if (mysqlpp::StoreQueryResult res = query.store()) {
			for (mysqlpp::StoreQueryResult::const_iterator it= res.begin(); it != res.end(); ++it) {
				std::string tbl 	= (*it)["data_type"].data();
				std::shared_ptr<aggreg_data> that = std::make_shared<aggreg_data>();
				std::map<std::string,std::shared_ptr<aggreg_data>>::iterator iter = tables.find(tbl);
				if (iter != tables.end())
					that = iter->second;
				that->delay_am		= (*it)["delay_am"];
				that->delay_ah		= (*it)["delay_ah"];
				that->delay_ad		= (*it)["delay_ad"];
				that->retention_d	= (*it)["retention_d"];
				that->retention_am	= (*it)["retention_am"];
				that->retention_ah	= (*it)["retention_ah"];
				that->retention_ad	= (*it)["retention_ad"];
				if (iter == tables.end())
					tables[tbl]	= that;
				else
					continue;

				if (that->delay_am < thread_freq) thread_freq = that->delay_am;
				std::string id_name = "host_id";
				if (tableHasColumn("d$"+tbl, "serv_id"))
					id_name = "serv_id";
				bool haveAM		= haveTable("am$"+tbl);
				bool haveAH		= haveTable("ah$"+tbl);
				bool haveAD		= haveTable("ad$"+tbl);
				bool haveAL		= haveTable("all$"+tbl);
				std::string create_am	= "create table am$"+tbl+" as select "+id_name+", res_id, floor(timestamp/60000)*60000.0000 as timestamp ";
				std::string create_ah	= "create table ah$"+tbl+" as select "+id_name+", res_id, floor(timestamp/3600000)*3600000.0000 as timestamp ";
				std::string create_ad	= "create table ad$"+tbl+" as select "+id_name+", res_id, floor(timestamp/86400000)*86400000.0000 as timestamp ";
				that->base_am		= "insert into am$"+tbl+" select d."+id_name+", d.res_id, floor(d.timestamp/60000)*60000.0000 as timestamp ";
				that->base_ah		= "insert into ah$"+tbl+" select d."+id_name+", d.res_id, floor(d.timestamp/3600000)*3600000.0000 as timestamp ";
				that->base_ad		= "insert into ad$"+tbl+" select d."+id_name+", d.res_id, floor(d.timestamp/86400000)*86400000.0000 as timestamp ";
				std::string create_al	= "create or replace view all$"+tbl+" as select ad.* from ad$"+tbl+" ad, (select min(timestamp) as mint, "+id_name+", res_id from ah$"+tbl+" group by "+id_name+", res_id) ah where ah."+id_name+" = ad."+id_name+" and ah.res_id  = ad.res_id and ad.timestamp > ah.mint union all select ah.* from ah$"+tbl+" ah, (select min(timestamp) as mint, "+id_name+", res_id from am$"+tbl+" group by "+id_name+", res_id) am where am."+id_name+" = ah."+id_name+" and am.res_id  = ah.res_id and ah.timestamp > am.mint union all select am.*  from am$"+tbl+" am, (select min(timestamp) as mint, "+id_name+", res_id from d$"+tbl+" group by "+id_name+", res_id) d where d."+id_name+" = am."+id_name+" and d.res_id  = am.res_id and am.timestamp > d.mint union all select "+id_name+", res_id, timestamp ";
				std::string colm;
				std::string colh;
				
				mysqlpp::Query qcols = db->query();
				
				qcols 	<< "select col.column_name from information_schema.columns col where col.TABLE_SCHEMA=DATABASE() and col.table_name = '" << "d$"+tbl 
					<< "' and col.data_type in ('int', 'double') and column_name not in ('"+id_name+"', 'res_id', 'timestamp')";
				try {
				if (mysqlpp::StoreQueryResult resc = qcols.store()) {
					for (mysqlpp::StoreQueryResult::const_iterator itc= resc.begin(); itc != resc.end(); ++itc) {
						std::string col = (*itc)[0].c_str();
						colm = ", avg("+col+") as avg_"+col+", min("+col+") as min_"+col+", max("+col+") as max_"+col+", sum("+col+") as sum_"+col+", count("+col+") as cnt_"+col+" ";
						that->base_am+=colm;
						if (!haveAM)
							create_am += colm;
						else if (!tableHasColumn("am$"+tbl,"avg_"+col)) {
							// TODO: add the missing avg,min,max columns
						}
						colh  = ", sum(sum_"+col+")/sum(cnt_"+col+") as avg_"+col+", min(min_"+col+") as min_"+col+", max(max_"+col+") as max_"+col+", sum(sum_"+col+") as sum_"+col+", sum(cnt_"+col+") as cnt_"+col+" ";
						that->base_ah+=colh;
						if (!haveAH)
							create_ah += colh;
						else if (!tableHasColumn("ah$"+tbl,"avg_"+col)) {
							// TODO: add the missing avg,min,max columns
						}
						that->base_ad+=colh;
						if (!haveAD)
							create_ad += colh;
						else if (!tableHasColumn("ad$"+tbl,"avg_"+col)) {
							// TODO: add the missing avg,min,max columns
						}
						if(!haveAL)
							create_al += ", "+col+" as avg_"+col+", "+col+" as min_"+col+", "+col+" as max_"+col+", "+col+" as sum_"+col+", 1 as cnt_"+col;
					}
					that->base_am+= "from d$"+tbl+" d, (select u."+id_name+", u.res_id, max(u.timestamp) as timestamp from (select x."+id_name+", x.res_id, max(x.timestamp) as timestamp from am$"+tbl+" x group by x."+id_name+", x.res_id union all select z."+id_name+", z.res_id, 0.0000 as timestamp from d$"+tbl+" z group by z."+id_name+", z.res_id) u group by u."+id_name+", u.res_id) y where floor(d.timestamp/60000)<floor(UNIX_TIMESTAMP(now())/60)-"+std::to_string(that->delay_am)+" and floor(d.timestamp/60000)*60000>y.timestamp and d."+id_name+"=y."+id_name+" and d.res_id=y.res_id group by d."+id_name+",d.res_id, floor(d.timestamp/60000)*60000.0000";
					that->base_ah+= "from am$"+tbl+" d, (select u."+id_name+", u.res_id, max(u.timestamp) as timestamp from (select x."+id_name+", x.res_id, max(x.timestamp) as timestamp from ah$"+tbl+" x group by x."+id_name+", x.res_id union all select z."+id_name+", z.res_id, 0.0000 as timestamp from am$"+tbl+" z group by z."+id_name+", z.res_id) u group by u."+id_name+", u.res_id) y where floor(d.timestamp/3600000)<floor(UNIX_TIMESTAMP(now())/3600)-"+std::to_string(that->delay_ah)+" and floor(d.timestamp/3600000)*3600000>y.timestamp and d."+id_name+"=y."+id_name+" and d.res_id=y.res_id group by d."+id_name+",d.res_id, floor(d.timestamp/3600000)*3600000.0000";
					that->base_ad+= "from ah$"+tbl+" d, (select u."+id_name+", u.res_id, max(u.timestamp) as timestamp from (select x."+id_name+", x.res_id, max(x.timestamp) as timestamp from ad$"+tbl+" x group by x."+id_name+", x.res_id union all select z."+id_name+", z.res_id, 0.0000 as timestamp from am$"+tbl+" z group by z."+id_name+", z.res_id) u group by u."+id_name+", u.res_id) y where floor(d.timestamp/86400000)<floor(UNIX_TIMESTAMP(now())/3600)-"+std::to_string(that->delay_ad)+" and floor(d.timestamp/86400000)*86400000>y.timestamp and d."+id_name+"=y."+id_name+" and d.res_id=y.res_id group by d."+id_name+",d.res_id, floor(d.timestamp/86400000)*86400000.0000";
					
					if(!haveAM) {
						create_am += "from d$"+tbl+" where floor(timestamp/60000)<floor(UNIX_TIMESTAMP(now())/60)-"+std::to_string(that->delay_am)+" group by "+id_name+",res_id, floor(timestamp/60000)*60000.0000";
						mysqlpp::Query qam = db->query(create_am);
						myqExec(qam, "statAggregator::init", "Failed to create aggregate AM")

						mysqlpp::Query qama = db->query("alter table am$"+tbl+" add constraint primary key ("+id_name+", res_id, timestamp)");
						myqExec(qama, "statAggregator::init", "Failed to alter aggregate AM")
						mysqlpp::Query qami = db->query("create index am$"+tbl+"_res_i on am$"+tbl+"(res_id)");
						myqExec(qami, "statAggregator::init", "Failed to index res_id on am$"+tbl)
						if (id_name == "host_id") {
							mysqlpp::Query qamc = db->query("alter table am$"+tbl+" add constraint am$"+tbl+"_"+id_name+"_i foreign key ("+id_name+") references h$ressources("+id_name+") on delete cascade on update cascade");
							myqExec(qamc, "statAggregator::init", "Failed to add constraint on am$"+tbl)
						} else {
							mysqlpp::Query qamc = db->query("alter table am$"+tbl+" add constraint am$"+tbl+"_"+id_name+"_i foreign key ("+id_name+") references s$ressources("+id_name+") on delete cascade on update cascade");
							myqExec(qamc, "statAggregator::init", "Failed to add constraint on am$"+tbl)
						}
					}
					if(!haveAH) {
						create_ah += "from am$"+tbl+" where floor(timestamp/3600000)<floor(UNIX_TIMESTAMP(now())/3600)-"+std::to_string(that->delay_ah)+" group by "+id_name+",res_id, floor(timestamp/3600000)*3600000.0000";
						mysqlpp::Query qah = db->query(create_ah);
						myqExec(qah, "statAggregator::init", "Failed to create aggregate AH")

						mysqlpp::Query qaha = db->query("alter table ah$"+tbl+" add constraint primary key ("+id_name+", res_id, timestamp)");
						myqExec(qaha, "statAggregator::init", "Failed to alter aggregate AH")
						mysqlpp::Query qahi = db->query("create index ah$"+tbl+"_res_i on ah$"+tbl+"(res_id)");
						myqExec(qahi, "statAggregator::init", "Failed to index res_id on ah$"+tbl)
						if (id_name == "host_id") {
							mysqlpp::Query qahc = db->query("alter table ah$"+tbl+" add constraint ah$"+tbl+"_"+id_name+"_i foreign key ("+id_name+") references h$ressources("+id_name+") on delete cascade on update cascade");
							myqExec(qahc, "statAggregator::init", "Failed to add constraint on ah$"+tbl)
						} else {
							mysqlpp::Query qamc = db->query("alter table ah$"+tbl+" add constraint ah$"+tbl+"_"+id_name+"_i foreign key ("+id_name+") references s$ressources("+id_name+") on delete cascade on update cascade");
							myqExec(qamc, "statAggregator::init", "Failed to add constraint on ah$"+tbl)
						}
					}
					if(!haveAD) {
						create_ad += "from ah$"+tbl+" where floor(timestamp/86400000)<floor(UNIX_TIMESTAMP(now())/3600)-"+std::to_string(that->delay_ad)+" group by "+id_name+",res_id, floor(timestamp/86400000)*86400000.0000";
						mysqlpp::Query qad = db->query(create_ad);
						myqExec(qad, "statAggregator::init", "Failed to create aggregate AD")

						mysqlpp::Query qada = db->query("alter table ad$"+tbl+" add constraint primary key ("+id_name+", res_id, timestamp)");
						myqExec(qada, "statAggregator::init", "Failed to alter aggregate AD")
						mysqlpp::Query qadi = db->query("create index ad$"+tbl+"_res_i on ad$"+tbl+"(res_id)");
						myqExec(qadi, "statAggregator::init", "Failed to index res_id on ah$"+tbl)
						if (id_name == "host_id") {
							mysqlpp::Query qadc = db->query("alter table ad$"+tbl+" add constraint ad$"+tbl+"_"+id_name+"_i foreign key ("+id_name+") references h$ressources("+id_name+") on delete cascade on update cascade");
							myqExec(qadc, "statAggregator::init", "Failed to add constraint on ad$"+tbl)
						} else {
							mysqlpp::Query qadc = db->query("alter table ad$"+tbl+" add constraint ad$"+tbl+"_"+id_name+"_i foreign key ("+id_name+") references s$ressources("+id_name+") on delete cascade on update cascade");
							myqExec(qadc, "statAggregator::init", "Failed to add constraint on ad$"+tbl)
						}
					}
					if(!haveAL) {
						create_al += " from d$"+tbl;
						mysqlpp::Query qal = db->query(create_al);
						myqExec(qal, "statAggregator::init", "Failed to create aggregate all view")
					}
				}
				} myqCatch(qcols, "statAggregator::init","Failed to get a data_table column")
			}
		}
		} myqCatch(query, "statAggregator::init","Failed to get data_table list")

		std::chrono::duration<double, std::milli> fp_ms = std::chrono::system_clock::now().time_since_epoch();
		for(std::map<std::string,std::shared_ptr<aggreg_data>>::iterator i = tables.begin();i != tables.end();i++) {
			mysqlpp::Query query = db->query();
			if (i->second->next_low < fp_ms) {
				query << i->second->base_am; //base_am[i->first];
				myqExec(query, "statAggregator::thread", "Failed to insert aggregate AM")
				if (i->second->retention_d>0) {
					query << "delete from d$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+std::to_string(i->second->retention_d)+")*"+std::to_string(sec2day*1000);
					myqExec(query, "statAggregator::thread", "Failed to purge raw data")
				}
				i->second->next_low  = fp_ms + std::chrono::minutes(i->second->delay_am);
			}
			if (i->second->next_med < fp_ms) {
				query << i->second->base_ah;
				myqExec(query, "statAggregator::thread", "Failed to insert aggregate AH")
				if (i->second->retention_am>0) {
					query <<"delete from am$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+std::to_string(i->second->retention_am)+")*"+std::to_string(sec2day*1000);
					myqExec(query, "statAggregator::thread", "Failed to purge aggregate AM")
				}
				i->second->next_med = fp_ms + std::chrono::hours(1);
			}
			if (i->second->next_high < fp_ms) {
				query << i->second->base_ad;
				myqExec(query, "statAggregator::thread", "Failed to insert aggregate AD")
				if (i->second->retention_ah>0) {
					query << "delete from ah$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+std::to_string(i->second->retention_ah)+")*"+std::to_string(sec2day*1000);
					myqExec(query, "statAggregator::thread", "Failed to purge aggregate AH")
				}
				if (i->second->retention_ad>0) {
					query << "delete from ad$"+i->first+" where timestamp<(floor(UNIX_TIMESTAMP(now())/"+std::to_string(sec2day)+")-"+std::to_string(i->second->retention_ad)+")*"+std::to_string(sec2day*1000);
					myqExec(query, "statAggregator::thread", "Failed to purge aggregate AH")
				}
				i->second->next_high = fp_ms + std::chrono::hours(12);
			}
		}
		mysqlpp::Connection::thread_end();
		std::this_thread::sleep_for(std::chrono::minutes(thread_freq));
	}
	});
}

}
