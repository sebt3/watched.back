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

## Planned
- Plugin (C++ or Lua) based trigger on events for alerting
