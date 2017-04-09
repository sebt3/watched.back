create table c$agents (
	id 		int(32) unsigned auto_increment,
	host 		varchar(256) not null, 
	port 		int(32) unsigned not null,
	pool_freq 	int(32) unsigned default 300,
	central_id 	int(32) not null default 1,
	use_ssl		bool default 0,
	constraint agent_pk primary key (id),
	constraint unique index agent_u (host, port)
);

create table agt$history (
	agt_id		int(32) unsigned not null,
	timestamp	double(20,4) unsigned not null,
	failed		int(32) unsigned not null,
	missing		int(32) unsigned not null,
	parse		int(32) unsigned not null,
	ok		int(32) unsigned not null,
	constraint unique index agtHistory_u(agt_id, timestamp),
	constraint fk_agtHistory_id foreign key(agt_id) references c$agents(id) on delete cascade on update cascade
);

create table c$ressources (
	id		int(32)	unsigned auto_increment, 
	name		varchar(256) not null,
	origin		varchar(256) not null,
	data_type	varchar(256) not null,
	constraint ressources_pk primary key (id),
	constraint unique index ressources_u (data_type, name, origin)
);

create table c$properties (
	id		int(32)	unsigned auto_increment, 
	name		varchar(256) not null,
	constraint properties_pk primary key (id),
	constraint unique index properties_name_u (name)
);

create table c$domains(
	id		int(32) unsigned auto_increment,
	name		varchar(256) not null,
	constraint domaines_pk primary key (id),
	constraint unique index domaines_u (name)
);

create table c$event_types(
	id		int(32) unsigned auto_increment, 
	name		varchar(256) not null,
	constraint event_types_pk primary key(id),
	constraint unique index event_types_u(name)
);

create table c$aggregate_config(
	name		varchar(256) not null,
	delay_am	int(32) unsigned not null default 30,
	delay_ah	int(32) unsigned not null default 2,
	delay_ad	int(32) unsigned not null default 1,
	retention_d	int(32) unsigned not null default 14,
	retention_am	int(32) unsigned not null default 60,
	retention_ah	int(32) unsigned not null default 730,
	retention_ad	int(32) unsigned not null default 0,
	constraint c$aggregate_config_pk primary key(name)
);
insert into c$aggregate_config(name) values('default');

create or replace view c$data_tables as
select def.data_type, def.drive, t.table_name as 'data_table', m.table_name as 'aggregate_min', h.table_name as 'aggregate_hour', d.table_name as 'aggregate_day' 
  from (select distinct data_type, concat('d$',data_type) as data_table, concat('am$',data_type) as am_table, concat('ah$',data_type) as ah_table, concat('ad$',data_type) as ad_table, concat(substr(origin,1,4),'_id') as drive
	  from c$ressources) def
left join (select table_name from information_schema.tables where TABLE_SCHEMA=DATABASE()) t on t.table_name=def.data_table
left join (select table_name from information_schema.tables where TABLE_SCHEMA=DATABASE()) h on h.table_name=def.ah_table
left join (select table_name from information_schema.tables where TABLE_SCHEMA=DATABASE()) m on m.table_name=def.am_table
left join (select table_name from information_schema.tables where TABLE_SCHEMA=DATABASE()) d on d.table_name=def.ad_table;

create or replace view c$data_sizes as
select def.data_type, ifnull(t.size,0) + ifnull(m.size,0) + ifnull(h.size,0) + ifnull(d.size,0) as total_size, t.size as 'data_size', m.size as 'min_size', h.size as 'hour_size', d.size as 'day_size',
	ch.cardinality as 'host_card', cs.cardinality as 'serv_card', cr.cardinality/(ifnull(ch.cardinality,cs.cardinality)) as 'res_card',
	t.table_rows as 'data_rows', m.table_rows as 'min_rows', h.table_rows as 'hour_rows', d.table_rows as 'days_rows',
	t.avg_row_length as 'data_avg', m.avg_row_length as 'min_avg', h.avg_row_length as 'hour_avg', d.avg_row_length as 'days_avg'
  from (select distinct data_type, concat('d$',data_type) as data_table, concat('am$',data_type) as am_table, concat('ah$',data_type) as ah_table, concat('ad$',data_type) as ad_table 
	  from c$ressources) def
left join (select table_name, (data_length + index_length)/pow(2,20) as "size", table_rows, avg_row_length from information_schema.tables where TABLE_SCHEMA=DATABASE()) t on t.table_name=def.data_table
left join (select table_name, (data_length + index_length)/pow(2,20) as "size", table_rows, avg_row_length from information_schema.tables where TABLE_SCHEMA=DATABASE()) h on h.table_name=def.ah_table
left join (select table_name, (data_length + index_length)/pow(2,20) as "size", table_rows, avg_row_length from information_schema.tables where TABLE_SCHEMA=DATABASE()) m on m.table_name=def.am_table
left join (select table_name, (data_length + index_length)/pow(2,20) as "size", table_rows, avg_row_length from information_schema.tables where TABLE_SCHEMA=DATABASE()) d on d.table_name=def.ad_table
left join (select table_name, cardinality from information_schema.statistics where table_schema=database() and column_name='host_id') ch on ch.table_name=def.data_table
left join (select table_name, cardinality from information_schema.statistics where table_schema=database() and column_name='serv_id') cs on cs.table_name=def.data_table
left join (select table_name, cardinality from information_schema.statistics where table_schema=database() and column_name='res_id') cr on cr.table_name=def.data_table;

create or replace view c$data_configs as
select	d.data_type, ifnull(a.delay_am, c.delay_am) delay_am,  ifnull(a.delay_ah, c.delay_ah) delay_ah, ifnull(a.delay_ad, c.delay_ad) delay_ad, 
	ifnull(a.retention_d, c.retention_d) retention_d, ifnull(a.retention_am, c.retention_am) retention_am, 
	ifnull(a.retention_ah, c.retention_ah) retention_ah, ifnull(a.retention_ad, c.retention_ad) retention_ad
  from (select * from c$aggregate_config where name = 'default') c, (select distinct data_type from c$ressources) d
left join c$aggregate_config a on d.data_type=a.name;


create table h$hosts(
	id		int(32) unsigned auto_increment,
	name		varchar(256) not null,
	domain_id	int(32),
	constraint hosts_pk primary key(id),
	constraint unique index hosts_name_u(name)
);

create table h$ressources(
	host_id		int(32) unsigned, 
	res_id		int(32) unsigned, 
	constraint host_ressources_pk primary key (host_id,res_id),
	constraint fk_host_ressources_hostid foreign key(host_id) references h$hosts(id)      on delete cascade on update cascade,
	constraint fk_host_ressources_resid  foreign key(res_id)  references c$ressources(id) on delete cascade on update cascade
);
create index host_ressources_hostid_i on h$ressources(host_id);
create index host_ressources_resid_i on h$ressources(res_id);

create table h$event_factory(
	host_id		int(32) unsigned,
	res_id		int(32) unsigned,
	res_type	varchar(256),
	event_type 	int(32) unsigned not null,
	property	varchar(256) not null,
	oper		char not null,
	value		double(20,4) not null,
	constraint fk_host_factory_hostid foreign key(host_id) references h$hosts(id)      on delete cascade on update cascade,
	constraint fk_host_factory_resid  foreign key(res_id)  references c$ressources(id) on delete cascade on update cascade
);
create index host_factory_hostid_i on h$event_factory(host_id);
create index host_factory_resid_i on h$event_factory(res_id);

create table h$res_events(
	id		int(32) unsigned auto_increment,
	host_id		int(32) unsigned not null,
	res_id		int(32) unsigned not null,
	start_time	double(20,4) unsigned not null,
	end_time	double(20,4) unsigned,
	event_type	int(32) unsigned not null,
	property	varchar(256) not null,
	current_value	double(20,4) not null,
	oper		char not null,
	value		double(20,4) not null,
	constraint events_pk primary key(id),
	constraint fk_host_events_ressources foreign key(host_id, res_id) references h$ressources(host_id, res_id) on delete cascade on update cascade
);
create index host_events_ressources_i on h$res_events(host_id, res_id);

create or replace view h$monitoring_items as
select ar.*, r.name as res_name, r.origin, r.data_type as res_type, ef.host_id as factory_host_id, ef.res_id as factory_res_id, ef.res_type as factory_res_type, ef.event_type, et.name as event_name, ef.property, ef.oper, ef.value
  from c$ressources r, h$ressources ar, h$event_factory ef, c$event_types et
 where ar.res_id=r.id
   and ef.event_type = et.id
   and (ef.host_id =ar.host_id or ef.host_id is null) and (ef.res_id=ar.res_id or ef.res_id is null) and (ef.res_type=r.data_type or ef.res_type is null);

create table s$types (
	id		int(32) unsigned auto_increment,
	name		varchar(256) not null,
	constraint serv_types_pk primary key(id),
	constraint unique index serv_types_u(name)
);
create table s$services (
	id		int(32) unsigned auto_increment,
	host_id		int(32) unsigned,
	type_id		int(32) unsigned,
	name		varchar(256) not null,
	constraint services_pk primary key(id),
	constraint unique index services_u(host_id, name),
	constraint fk_services_hostid foreign key(host_id) references h$hosts(id) on delete cascade on update cascade
);
create index services_hostid_i on s$services(host_id);

create table s$ressources(
	serv_id		int(32) unsigned not null,
	res_id		int(32) unsigned not null,
	constraint services_ressources_pk primary key (serv_id,res_id),
	constraint fk_services_ressources_servid foreign key(serv_id) references s$services(id)   on delete cascade on update cascade,
	constraint fk_services_ressources_resid  foreign key(res_id)  references c$ressources(id) on delete cascade on update cascade
);
create index services_ressources_servid_i on s$ressources(serv_id);
create index services_ressources_resid_i on s$ressources(res_id);

create table s$event_factory(
	serv_id		int(32) unsigned,
	res_id		int(32) unsigned,
	res_type	varchar(256),
	event_type 	int(32) unsigned not null,
	property	varchar(256) not null,
	oper		char not null,
	value		double(20,4) not null,
	constraint fk_services_factory_servid foreign key(serv_id) references s$services(id)   on delete cascade on update cascade,
	constraint fk_services_factory_resid  foreign key(res_id)  references c$ressources(id) on delete cascade on update cascade
);
create index services_factory_servid_i on s$event_factory(serv_id);
create index services_factory_resvid_i on s$event_factory(res_id);

create table s$res_events(
	id		int(32) unsigned auto_increment,
	serv_id		int(32) unsigned not null,
	res_id		int(32) unsigned not null,
	start_time	double(20,4) unsigned not null,
	end_time	double(20,4) unsigned,
	event_type	int(32) unsigned not null,
	property	varchar(256) not null,
	current_value	double(20,4) not null,
	oper		char not null,
	value		double(20,4) not null,
	constraint services_res_events_pk primary key(id),
	constraint fk_services_events_ressources foreign key(serv_id, res_id) references s$ressources(serv_id, res_id) on delete cascade on update cascade
);
create index services_events_ressources_i on s$res_events(serv_id, res_id);

create table s$log_events(
	id		int(32) unsigned auto_increment,
	serv_id		int(32) unsigned not null,
	timestamp	double(20,4) unsigned not null,
	event_type	int(32) unsigned not null,
	source_name	varchar(256) not null,
	date_field	varchar(256) not null,
	line_no		int(32) unsigned not null,
	text		varchar(4096),
	constraint services_log_events_pk primary key(id),
	constraint unique index services_log_events_u(serv_id,source_name,date_field,line_no),
	constraint fk_services_log_events_servid foreign key(serv_id) references s$services(id)   on delete cascade on update cascade
);
create index services_log_events_servid_i on s$log_events(serv_id);

create table s$sockets (
	serv_id		int(32) unsigned  not null,
	name		varchar(256) not null,
	status		varchar(256),
	timestamp	double(20,4) unsigned,
	constraint unique index serviceSockets_u(serv_id, name),
	constraint fk_serviceSockets_id foreign key(serv_id) references s$services(id) on delete cascade on update cascade
);

create table s$process (
	serv_id		int(32) unsigned not null,
	name		varchar(256) not null,
	full_path	varchar(256),
	cwd		varchar(256),
	username	varchar(256),
	pid		int(32) unsigned,
	status		varchar(256),
	timestamp	double(20,4) unsigned,
	constraint unique index serviceProcess_u(serv_id, name),
	constraint fk_serviceProcess_id foreign key(serv_id) references s$services(id) on delete cascade on update cascade
);

create table s$history (
	serv_id		int(32) unsigned not null,
	timestamp	double(20,4) unsigned not null,
	failed		int(32) unsigned not null,
	missing		int(32) unsigned not null,
	ok		int(32) unsigned not null,
	constraint unique index serviceHistory_u(serv_id, timestamp),
	constraint fk_serviceHistory_id foreign key(serv_id) references s$services(id) on delete cascade on update cascade
);

create or replace view s$failed as
select min(timestamp) as timestamp, status, serv_id from s$sockets where (UNIX_TIMESTAMP()*1000-timestamp)/1000>60*15 or status not like 'ok%' group by status, serv_id
union
select min(timestamp) as timestamp, status, serv_id from s$process where (UNIX_TIMESTAMP()*1000-timestamp)/1000>60*15 or status not like 'ok%' group by status, serv_id;

create or replace view s$missing as
select x.serv_id as id, s.name, s.host_id, max(late_sec) as late_sec, count(distinct status) as cnt_stat, min(status) as status
  from (
	select (UNIX_TIMESTAMP()*1000-min(timestamp))/1000 as late_sec, status, serv_id
	  from s$sockets
	 group by status, serv_id
	union
	select (UNIX_TIMESTAMP()*1000-min(timestamp))/1000 as late_sec, status, serv_id
	  from s$process
	 group by status, serv_id
  ) x, s$services s
 where x.serv_id = s.id
   and late_sec>60*15
 group by x.serv_id, s.name;

create table g$groups(
	id		int(32) unsigned auto_increment,
	name		varchar(256) not null,
	constraint groups_pk primary key(id),
	constraint unique index groups_name_u(name)
);

create table a$apps(
	id		int(32) unsigned auto_increment,
	group_id	int(32) unsigned,
	name		varchar(256) not null,
	constraint apps_pk primary key(id),
	constraint unique index apps_name_u(name),
	constraint fk_apps_group foreign key(group_id) references g$groups(id) on delete set null on update cascade
);
create index apps_group_i on a$apps(group_id);

create table a$services(
	app_id		int(32) unsigned not null,
	serv_id		int(32) unsigned not null,
	constraint apps_services_pk primary key(app_id,serv_id),
	constraint fk_apps_services_app foreign key(app_id) references a$apps(id) on delete cascade on update cascade,
	constraint fk_apps_services_serv foreign key(serv_id) references s$services(id) on delete cascade on update cascade
);
create index apps_services_serv on a$services(serv_id);


create table b$backend(
	id		int(32) unsigned not null auto_increment,
	hostname	varchar(256) not null,
	filename	varchar(256) not null,
	constraint backend_pk primary key(id),
	constraint unique index backend_u(hostname,filename)
);

create table b$history (
	back_id		int(32) unsigned not null,
	timestamp	double(20,4) unsigned not null,
	failed		int(32) unsigned not null,
	ok		int(32) unsigned not null,
	constraint unique index backHistory_u(back_id, timestamp),
	constraint fk_backHistory_id foreign key(back_id) references b$backend(id) on delete cascade on update cascade
);


create table p$roles(
	id		int(32) unsigned auto_increment,
	name		varchar(256) not null,
	serv_type_id	int(32) unsigned,
	constraint perm_roles_pk primary key(id),
	constraint unique index perm_roles_name_u(name)
);

create table p$teams(
	id		int(32) unsigned auto_increment,
	name		varchar(256) not null,
	superadmin	boolean,
	constraint perm_teams_pk primary key(id),
	constraint unique index perm_teams_name_u(name)
);

create table p$team_properties (
	team_id		int(32) unsigned not null,
	prop_id		int(32) unsigned not null,
	value		varchar(1024) not null,
	constraint team_properties_pk primary key(team_id,prop_id),
	constraint fk_team_properties_team foreign key(team_id) references p$teams(id) on delete cascade on update cascade,
	constraint fk_team_properties_prop foreign key(prop_id) references c$properties(id) on delete cascade on update cascade
);
create index team_properties_prop_i on p$team_properties(prop_id);

create table p$domains (
	domain_id	int(32) unsigned not null,
	team_id		int(32) unsigned not null,
	role_id		int(32) unsigned not null,
	alert		boolean,
	constraint perm_domains_pk primary key(domain_id,team_id,role_id),
	constraint fk_perm_domains_domain foreign key(domain_id) references c$domains(id) on delete cascade on update cascade,
	constraint fk_perm_domains_team foreign key(team_id) references p$teams(id) on delete cascade on update cascade,
	constraint fk_perm_domains_role foreign key(role_id) references p$roles(id) on delete cascade on update cascade
);
create index perm_domains_team_i on p$domains(team_id);
create index perm_domains_role_i on p$domains(role_id);

create table p$hosts (
	host_id		int(32) unsigned not null,
	team_id		int(32) unsigned not null,
	role_id		int(32) unsigned not null,
	alert		boolean,
	constraint perm_hosts_pk primary key(host_id,team_id,role_id),
	constraint fk_perm_hosts_host foreign key(host_id) references h$hosts(id) on delete cascade on update cascade,
	constraint fk_perm_hosts_team foreign key(team_id) references p$teams(id) on delete cascade on update cascade,
	constraint fk_perm_hosts_role foreign key(role_id) references p$roles(id) on delete cascade on update cascade
);
create index perm_hosts_team_i on p$hosts(team_id);
create index perm_hosts_role_i on p$hosts(role_id);

create table p$services (
	serv_id		int(32) unsigned not null,
	team_id		int(32) unsigned not null,
	role_id		int(32) unsigned not null,
	alert		boolean,
	constraint perm_services_pk primary key(serv_id,team_id,role_id),
	constraint fk_perm_services_serv foreign key(serv_id) references s$services(id) on delete cascade on update cascade,
	constraint fk_perm_services_team foreign key(team_id) references p$teams(id) on delete cascade on update cascade,
	constraint fk_perm_services_role foreign key(role_id) references p$roles(id) on delete cascade on update cascade
);
create index perm_services_team_i on p$services(team_id);
create index perm_services_role_i on p$services(role_id);

create table p$groups (
	group_id	int(32) unsigned not null,
	team_id		int(32) unsigned not null,
	role_id		int(32) unsigned not null,
	alert		boolean,
	constraint perm_groups_pk primary key(group_id,team_id,role_id),
	constraint fk_perm_groups_group foreign key(group_id) references g$groups(id) on delete cascade on update cascade,
	constraint fk_perm_groups_team foreign key(team_id) references p$teams(id) on delete cascade on update cascade,
	constraint fk_perm_groups_role foreign key(role_id) references p$roles(id) on delete cascade on update cascade
);
create index perm_groups_team_i on p$groups(team_id);
create index perm_groups_role_i on p$groups(role_id);

create table p$apps (
	app_id		int(32) unsigned not null,
	team_id		int(32) unsigned not null,
	role_id		int(32) unsigned not null,
	alert		boolean,
	constraint perm_apps_pk primary key(team_id,role_id),
	constraint fk_perm_apps_app foreign key(app_id) references a$apps(id) on delete cascade on update cascade,
	constraint fk_perm_apps_team foreign key(team_id) references p$teams(id) on delete cascade on update cascade,
	constraint fk_perm_apps_role foreign key(role_id) references p$roles(id) on delete cascade on update cascade
);
create index perm_apps_team_i on p$apps(team_id);
create index perm_apps_role_i on p$apps(role_id);


create or replace view p$hosts_direct_teams as
select h.host_id, h.team_id, h.role_id, h.alert
  from p$hosts h
union
select hh.id as host_id, d.team_id, d.role_id, d.alert
  from p$domains d, h$hosts hh
 where hh.domain_id = d.domain_id;

create or replace view p$services_direct_teams as
select s.serv_id, s.team_id, s.role_id, s.alert
  from p$services s
union
select sa.serv_id, a.team_id, a.role_id, a.alert
  from p$apps a, a$services sa
 where sa.app_id = a.app_id
union
select sa.serv_id, g.team_id, g.role_id, g.alert
  from p$groups g, a$apps a, a$services sa
 where sa.app_id = a.id
   and g.group_id= a.group_id;

create or replace view p$teams_all_hosts as
select hdt.team_id, hdt.host_id, hdt.role_id, hdt.alert
  from p$hosts_direct_teams hdt
union
select sdt.team_id, s.host_id, sdt.role_id, sdt.alert
  from p$services_direct_teams sdt, s$services s
 where s.id=sdt.serv_id
union 
select t.id as team_id, h.id as host_id, null as role_id, null as alert
  from p$teams t, h$hosts h
 where t.superadmin=true;

create or replace view p$teams_all_services as
select sdt.team_id, sdt.serv_id, sdt.role_id, sdt.alert
  from p$services_direct_teams sdt
union
select hdt.team_id, s.id as serv_id, hdt.role_id, hdt.alert
  from p$hosts_direct_teams hdt, s$services s, p$roles r
 where s.host_id=hdt.host_id
   and r.id = hdt.role_id
   and (r.serv_type_id is null or r.serv_type_id=s.type_id)
union 
select t.id as team_id, s.id as host_id, null as role_id, null as alert
  from p$teams t, s$services s
 where t.superadmin=true;

create table u$users (
	id		int(32) unsigned auto_increment,
	username	varchar(256) not null,
	firstname	varchar(128),
	lastname	varchar(128),
	passhash	varchar(512),
	constraint users_pk primary key(id),
	constraint unique index users_name_u(username)
);

create table u$tokens (
	id		int(32) unsigned auto_increment,
	user_id		int(32) unsigned not null,
	keyname		varchar(256) not null,
	passhash	varchar(512) not null,
	created		timestamp default current_timestamp,
	constraint tokens_pk primary key(id),
	constraint unique index tokens_name_u(keyname),
	constraint fk_tokens_user 	foreign key(user_id) references u$users(id) on delete cascade on update cascade
);
create index tokens_user_i on u$tokens(user_id);

create table u$teams (
	user_id		int(32) unsigned not null,
	team_id		int(32) unsigned not null,
	constraint users_teams_pk primary key(user_id,team_id),
	constraint fk_users_teams_user foreign key(user_id) references u$users(id) on delete cascade on update cascade,
	constraint fk_users_teams_team foreign key(team_id) references p$teams(id) on delete cascade on update cascade
);
create index users_teams_team_i on u$teams(team_id);

create table u$properties (
	user_id		int(32) unsigned not null,
	prop_id		int(32) unsigned not null,
	value		varchar(1024) not null,
	constraint users_properties_pk primary key(user_id,prop_id),
	constraint fk_users_properties_user foreign key(user_id) references u$users(id) on delete cascade on update cascade,
	constraint fk_users_properties_prop foreign key(prop_id) references c$properties(id) on delete cascade on update cascade
);
create index users_properties_prop_i on u$properties(prop_id);

create or replace view p$users_all_hosts as
select ut.user_id, th.host_id, th.role_id
  from p$teams_all_hosts th, u$teams ut
 where ut.team_id=th.team_id;

create or replace view p$users_all_domains as
select distinct ut.user_id, h.domain_id, th.role_id
  from p$teams_all_hosts th, u$teams ut, h$hosts h
 where ut.team_id=th.team_id
   and th.host_id=h.id;

create or replace view p$users_all_services as
select ut.user_id, ts.serv_id, ts.role_id
  from p$teams_all_services ts, u$teams ut
 where ut.team_id=ts.team_id;

create or replace view p$users_all_apps as
select distinct ut.user_id, a.app_id, ts.role_id
  from p$teams_all_services ts, u$teams ut, a$services a
 where ut.team_id=ts.team_id
   and ts.serv_id=a.serv_id;

create or replace view p$users_all_groups as
select distinct ut.user_id, aa.group_id, ts.role_id
  from p$teams_all_services ts, u$teams ut, a$services a, a$apps aa
 where ut.team_id=ts.team_id
   and ts.serv_id=a.serv_id
   and a.app_id=aa.id;

create or replace view p$users_admin as
select u.user_id, t.name as team_name, u.team_id
  from u$teams u, p$teams t
 where t.superadmin=true
   and t.id=u.team_id;

create or replace view p$alert_hosts as
select h.host_id, tph.prop_id, tph.value
  from p$team_properties tph, p$hosts h, p$roles rh
 where h.alert=true
   and h.team_id=tph.team_id
   and rh.id=h.role_id
   and rh.serv_type_id is null
union
select hh.id as host_id, tpd.prop_id, tpd.value
  from p$domains d, h$hosts hh, p$team_properties tpd, p$roles rd
 where d.domain_id=hh.domain_id
   and d.team_id=tpd.team_id
   and d.role_id=rd.id
   and d.alert=true
   and rd.serv_type_id is null
union
select h.host_id, p.prop_id, p.value
  from u$properties p, u$teams t, p$hosts h, p$roles rh
 where p.user_id=t.user_id
   and t.team_id=h.team_id
   and h.role_id=rh.id
   and rh.serv_type_id is null
   and h.alert=true
union
select hh.id as host_id, up.prop_id, up.value
  from p$domains d, h$hosts hh, u$properties up, p$roles rd, u$teams ut
 where d.domain_id=hh.domain_id
   and d.team_id=ut.team_id
   and ut.user_id=up.user_id
   and d.role_id=rd.id
   and d.alert=true
   and rd.serv_type_id is null;

create or replace view p$alert_services as
select s.serv_id, tps.prop_id, tps.value
  from p$services s, p$team_properties tps
 where s.team_id=tps.team_id
   and s.alert=true
union
select sa.serv_id, tps.prop_id, tps.value
  from p$apps a, a$services sa, p$roles r, p$team_properties tps, s$services s
 where a.team_id=tps.team_id
   and a.role_id=r.id
   and a.alert=true
   and sa.serv_id=s.id
   and r.serv_type_id=s.type_id
   and sa.app_id = a.app_id
union
select s.id as serv_id, tps.prop_id, tps.value
  from p$hosts_direct_teams hdt, p$roles r, s$services s, p$team_properties tps
 where r.id=hdt.role_id
   and s.type_id=r.serv_type_id
   and hdt.team_id=tps.team_id
union
select s.serv_id, p.prop_id, p.value
  from u$properties p, u$teams t, p$services s
 where p.user_id=t.user_id
   and t.team_id=s.team_id
   and s.alert=true
union
select sa.serv_id, p.prop_id, p.value
  from p$apps a, a$services sa, p$roles r, u$properties p, u$teams t, s$services s
 where p.user_id=t.user_id
   and t.team_id=a.team_id
   and a.role_id=r.id
   and a.alert=true
   and sa.serv_id=s.id
   and r.serv_type_id=s.type_id
   and sa.app_id = a.app_id
union
select s.id as serv_id, p.prop_id, p.value
  from p$hosts_direct_teams hdt, p$roles r, s$services s, u$properties p, u$teams t
 where r.id=hdt.role_id
   and s.type_id=r.serv_type_id
   and p.user_id=t.user_id
   and t.team_id=hdt.team_id;

insert into c$domains(name) values ('Production'),('Qualification'),('Testing'),('Developpement');
insert into c$agents(host,port) values('localhost',9080);
insert into c$event_types(name) values ('Critical'),('Error'),('Warning'),('Notice'),('Information');
insert into h$event_factory(res_type, event_type, property, oper, value) values ('disk_usage', 3, 'pctfree', '<', 25),('disk_usage', 2, 'pctfree', '<', 5),('disk_usage', 2, 'ipctfree', '<', 5),('disk_usage', 3, 'ipctfree', '<', 25),('cpu_usage', 5, 'user', '>', 90);
insert into c$properties(name) values ('email');
insert into u$users(username) values ('public');
insert into u$users(username,passhash) values ('admin','$2y$10$GG4XXB9YGIZYZ6anAAZN1etXSldQqb1v7uO8p4H1r4eFOdkk80znW');
insert into s$types(name) values ('database'),('web'),('system');
insert into p$teams(name, superadmin) values ('admin', true);
insert into p$teams(name) values ('public'),('Production dba'),('Qualification dba'),('Testing dba'),('Developpement dba'),('Production sysadmin'),('Qualification sysadmin'),('Testing sysadmin'),('Developpement sysadmin'),('Production webadmin'),('Qualification webadmin'),('Testing webadmin'),('Developpement webadmin');
insert into p$roles(name) values ('hostadmin');
insert into p$roles(name, serv_type_id)
select 'dba' as name, t.id serv_type_id
  from s$types t
 where t.name='database'
union
select 'sysadmin' as name, t.id serv_type_id
  from s$types t
 where t.name='system'
union
select 'webadmin' as name, t.id serv_type_id
  from s$types t
 where t.name='web';
insert into u$teams(user_id, team_id)
select u.id as user_id, t.id as team_id
  from u$users u, p$teams t
 where u.username='admin'
   and t.name='admin';
insert into p$domains(domain_id, team_id, role_id, alert)
select d.id as domain_id, t.id as team_id, r.id as role_id, true
  from c$domains d, p$teams t, p$roles r
 where d.name='Production'
   and t.name='Production dba'
   and r.name='dba'
union
select d.id as domain_id, t.id as team_id, r.id as role_id, true
  from c$domains d, p$teams t, p$roles r
 where d.name='Production'
   and t.name='Production webadmin'
   and r.name='webadmin'
union
select d.id as domain_id, t.id as team_id, r.id as role_id, true
  from c$domains d, p$teams t, p$roles r
 where d.name='Production'
   and t.name='Production sysadmin'
   and r.name in ('sysadmin', 'hostadmin');

insert into p$domains(domain_id, team_id, role_id)
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Developpement'
   and t.name='Developpement dba'
   and r.name='dba'
union
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Qualification'
   and t.name='Qualification dba'
   and r.name='dba'
union
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Testing'
   and t.name='Testing dba'
   and r.name='dba'
union
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Developpement'
   and t.name='Developpement webadmin'
   and r.name='webadmin'
union
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Qualification'
   and t.name='Qualification webadmin'
   and r.name='webadmin'
union
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Testing'
   and t.name='Testing webadmin'
   and r.name='webadmin'
union
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Developpement'
   and t.name='Developpement sysadmin'
   and r.name in ('sysadmin', 'hostadmin')
union
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Qualification'
   and t.name='Qualification sysadmin'
   and r.name in ('sysadmin', 'hostadmin')
union
select d.id as domain_id, t.id as team_id, r.id as role_id
  from c$domains d, p$teams t, p$roles r
 where d.name='Testing'
   and t.name='Testing sysadmin'
   and r.name in ('sysadmin', 'hostadmin');
commit;
