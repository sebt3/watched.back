# watched.back
[![Status](https://travis-ci.org/sebt3/watched.back.svg?branch=master)](https://travis-ci.org/sebt3/watched.back)
## Overview
[Watched](https://sebt3.github.io/watched/) aim to become a performance analysis and a monitoring tool.
The watched backend collect all the performance metrics and services status from the agents. It is scalable, secure and highly-available already.

## Dependencies
This project is based on the following projects :
* [mysql++](https://tangentsoft.net/mysql++/)
* [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
* [SimpleWebServer](https://github.com/eidheim/Simple-Web-Server) (itself based on Boost::asio)
* [Boost](http://www.boost.org) version >= 1.54
* C++11 compatible compiler (gcc >=4.9, clang >=3.3)
* Linux

## Other componants
* [watched.agent](https://github.com/sebt3/watched.agent) Collect metrics and monitor services forwarding the information over a REST api.
* [watched.front](https://github.com/sebt3/watched.front) Provide the interface to the centralized data

## build instruction
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
Complete instructions [here](https://sebt3.github.io/watched/doc/install/#build-the-backend).

## Current features
- Collect the agents data and store that in a Mysql database
- Compute aggregated data
- Monitor ressources values and create event in the database if it cross configured limits
- Collect service log and create log events in the database
- Collect services status and update the database accordingly and historize
- Collect services metrics
- Scalable : the work-load can be splitted in sub-groups of backend
- Secure : support SSL  agents and can conect to mysql over SSL
- Highly Available
- Plugin (C++ or Lua) for alerting
  * email

## TODO
- Detect missing Host and Agent
- Code improvement (agentClient.cpp:38)
- Request for an updated API for known agents at configured interval (agentManager.cpp:41)
- Add support for alert on Host failed (alerter.cpp:186)
- Add support for alert on agent failed (alerter.cpp:188)
- Add a configuration option for agents updates (backend.cpp:23)
- Support the since flag (servicesClient.cpp:12)
- Add support for ad$ tables (statAggregator.cpp)
- Update aggregate tables with missing columns (statAggregator.cpp:69), (statAggregator.cpp:76)
- Add foreign keys on aggregate table to (s|h)$ressources (statAggregator.cpp:89), (statAggregator.cpp:99)
- add support for sigusr1 which reload configs et reopen log

