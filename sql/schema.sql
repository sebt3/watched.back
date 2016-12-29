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

create table c$ressources (
	id		int(32)	unsigned auto_increment, 
	name		varchar(256) not null,
	origin		varchar(256) not null,
	data_type	varchar(256) not null,
	constraint ressources_pk primary key (id),
	constraint unique index ressources_u (data_type, name, origin)
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

create view c$data_tables as select distinct data_type as table_name from c$ressources;

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

create view h$monitoring_items as
select ar.*, r.name as res_name, r.origin, r.data_type as res_type, ef.host_id as factory_host_id, ef.res_id as factory_res_id, ef.res_type as factory_res_type, ef.event_type, et.name as event_name, ef.property, ef.oper, ef.value
  from c$ressources r, h$ressources ar, h$event_factory ef, c$event_types et
 where ar.res_id=r.id
   and ef.event_type = et.id
   and (ef.host_id =ar.host_id or ef.host_id is null) and (ef.res_id=ar.res_id or ef.res_id is null) and (ef.res_type=r.data_type or ef.res_type is null);

create table s$services (
	id		int(32) unsigned auto_increment,
	host_id		int(32) unsigned,
	name		varchar(256) not null,
	constraint hosts_pk primary key(id),
	constraint unique index services_u(host_id, name),
	constraint fk_services_hostid foreign key(host_id) references h$hosts(id) on delete cascade on update cascade
);

create table s$ressources(
	serv_id		int(32) unsigned not null,
	res_id		int(32) unsigned not null,
	constraint services_ressources_pk primary key (serv_id,res_id),
	constraint fk_services_ressources_servid foreign key(serv_id) references s$services(id)   on delete cascade on update cascade,
	constraint fk_services_ressources_resid  foreign key(res_id)  references c$ressources(id) on delete cascade on update cascade
);

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
	constraint events_pk primary key(id),
	constraint fk_services_events_ressources foreign key(serv_id, res_id) references s$ressources(serv_id, res_id) on delete cascade on update cascade
);

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
	CONSTRAINT fk_serviceHistory_id foreign key(serv_id) references s$services(id) on delete cascade on update cascade
);

create view s$failed as
select min(timestamp) as timestamp, status, serv_id from s$sockets where (UNIX_TIMESTAMP()*1000-timestamp)/1000>60*15 or status not like 'ok%' group by status, serv_id
union
select min(timestamp) as timestamp, status, serv_id from s$process where (UNIX_TIMESTAMP()*1000-timestamp)/1000>60*15 or status not like 'ok%' group by status, serv_id;

insert into c$domains(name) values ("Production"),("Qualification"),("Testing"),("Developpement");
insert into c$agents(host,port) values('localhost',9080);
insert into c$event_types(name) values ("Critical"),("Error"),("Warning"),("Notice"),("Information");
insert into h$event_factory(res_type, event_type, property, oper, value) values ('disk_usage', 3, 'pctfree', '<', 25),('disk_usage', 2, 'pctfree', '<', 5),('disk_usage', 2, 'ipctfree', '<', 5),('disk_usage', 3, 'ipctfree', '<', 25),('cpu_usage', 5, 'user', '>', 90);
commit;
