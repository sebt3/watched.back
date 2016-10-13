# watched.back
[![Status](https://travis-ci.org/sebt3/watched.back.svg?branch=master)](https://travis-ci.org/sebt3/watched.back)
## Overview
[Watched](https://sebt3.github.io/watched/) aim to become a performance analysis and a monitoring tool.
This is the backend. It collect all the performance metrics from the agents. It monitor the metrics, store the data in a mysql database and compute aggregates.

## Dependencies
This project is based on the following projects :
* [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
* [SimpleWebServer](https://github.com/eidheim/Simple-Web-Server) (itself based on Boost::asio)
* [mysql++](https://tangentsoft.net/mysql++/)

## Other componants
* [watched.agent](https://github.com/sebt3/watched.agent) the agent. Provide a REST api to the collected metrics.
* [watched.front](https://github.com/sebt3/watched.front) the frontend. 

## build instruction
    mkdir build
    cd build
    cmake ..
    make

## Current status
This is the very early days.
