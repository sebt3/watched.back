create table agents (
	id 		int(32) unsigned auto_increment,
	host 		varchar(256) not null, 
	port 		int(32) unsigned not null,
	pool_freq 	int(32) unsigned default 300,
	central_id 	int(32) not null default 1,
	domain_id	int(32),
	constraint agent_pk primary key (id),
	constraint unique index agent_u (host, port)
);
create table ressources (
	id int(32)	unsigned auto_increment, 
	name		varchar(256) not null,
	type		varchar(256) not null,
	constraint ressources_pk primary key (id),
	constraint unique index ressources_u (name)
);
create table host_ressources(
	host_id		int(32) unsigned, 
	res_id		int(32) unsigned, 
	constraint host_ressources_pk primary key (host_id,res_id)
);
create table domains(
	id		int(32) unsigned auto_increment,
	name		varchar(256) not null,
	constraint domaines_pk primary key (id)
);

create table event_types(
	id		int(32) unsigned auto_increment, 
	name		varchar(256) not null,
	constraint event_types_pk primary key(id),
	constraint unique index event_types_u(name)
);
create table event_factory(
	host_id		int(32) unsigned,
	res_id		int(32) unsigned,
	res_type	varchar(256),
	event_type 	int(32) unsigned not null,
	property	varchar(256) not null,
	oper		char not null,
	value		double(20,4) not null
);
create table res_events(
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
	constraint events_pk primary key(id)
);

create table hosts(
	id		int(32) unsigned auto_increment,
	name		varchar(256) not null,
	constraint hosts_pk primary key(id),
	constraint unique index hosts_name_u(name)
);

create table services (
	id		int(32) unsigned auto_increment,
	host_id		int(32) unsigned,
	name		varchar(256) not null,
	constraint hosts_pk primary key(id),
	constraint unique index services_u(host_id, name)
);

create table serviceSockets (
	serv_id		int(32) unsigned,
	name		varchar(256) not null,
	status		varchar(256),
	timestamp	double(20,4) unsigned,
	constraint unique index serviceSockets_u(serv_id, name)
);

create table serviceProcess (
	serv_id		int(32) unsigned,
	name		varchar(256) not null,
	full_path	varchar(256),
	cwd		varchar(256),
	username	varchar(256),
	pid		int(32) unsigned,
	status		varchar(256),
	timestamp	double(20,4) unsigned,
	constraint unique index serviceProcess_u(serv_id, name)
);

create view live_tables as select distinct type as table_name from ressources;
create view monitoring_items as
select ar.*, r.name as res_name, r.type as res_type, ef.host_id as factory_host_id, ef.res_id as factory_res_id, ef.res_type as factory_res_type, ef.event_type, et.name as event_name, ef.property, ef.oper, ef.value
  from ressources r, host_ressources ar, event_factory ef, event_types et
 where ar.res_id=r.id
   and ef.event_type = et.id
   and (ef.host_id =ar.host_id or ef.host_id is null) and (ef.res_id=ar.res_id or ef.res_id is null) and (ef.res_type=r.type or ef.res_type is null);


insert into domains(name) values ("Production"),("Qualification"),("Testing"),("Developpement");
insert into agents(host,port) values('localhost',9080);
insert into event_types(name) values ("Critical"),("Error"),("Warning"),("Notice"),("Information");
insert into event_factory(res_type, event_type, property, oper, value) values ('disk_usage', 3, 'pctfree', '<', 25),('disk_usage', 2, 'pctfree', '<', 5),('disk_usage', 2, 'ipctfree', '<', 5),('disk_usage', 3, 'ipctfree', '<', 25),('cpu_usage', 5, 'user', '>', 90);
commit;
