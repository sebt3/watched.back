cmake_minimum_required (VERSION 2.8.7)
project (watched.back)

####
## Forbid in-tree building
#
if(${CMAKE_SOURCE_DIR} MATCHES ${CMAKE_BINARY_DIR})
	message(STATUS "Please do an out-of-tree build:")
	message(STATUS "rm -f CMakeCache.txt && mkdir build && cd build; cmake .. && make")
	message(FATAL_ERROR "In-tree-build detected!")
endif(${CMAKE_SOURCE_DIR} MATCHES ${CMAKE_BINARY_DIR})


####
## Default installation path 
#
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX
		"/usr" CACHE PATH "Default install prefix" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

# Special case for /etc and /var when prefix is /usr
if(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
	set(CMAKE_INSTALL_SYSCONFDIR "/etc" CACHE PATH "read-only single-machine data (etc)")
	set(CMAKE_INSTALL_LOCALSTATEDIR "/var" CACHE PATH "modifiable single-machine data (var)")
else()
	set(CMAKE_INSTALL_SYSCONFDIR "${CMAKE_INSTALL_PREFIX}/etc" CACHE PATH "read-only single-machine data (etc)")
	set(CMAKE_INSTALL_LOCALSTATEDIR "${CMAKE_INSTALL_PREFIX}/var" CACHE PATH "modifiable single-machine data (var)")
endif(${CMAKE_INSTALL_PREFIX} STREQUAL "/usr")
include(GNUInstallDirs)

####
## C++11 Compiler 
#
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
if(COMPILER_SUPPORTS_CXX11)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
elseif(COMPILER_SUPPORTS_CXX0X)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler.")
endif()

####
## Compiler option
#
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC")
set(CMAKE_EXE_LINKER_FLAGS    "${CMAKE_EXE_LINKER_FLAGS}  -fPIC")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--as-needed -rdynamic")
set(CMAKE_BUILD_WITH_INSTALL_RPATH "ON")

####
## Dependencies
#
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
set(BOOST_COMPONENTS system thread filesystem date_time)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        set(BOOST_COMPONENTS ${BOOST_COMPONENTS} regex)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_BOOST_REGEX")
    endif()
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-depth=1024")
endif()
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
#find_package(Boost 1.54.0 COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
find_package(Asio REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Jsoncpp REQUIRED)
find_package(Mysqlpp REQUIRED)
find_package(Lua 5.2 REQUIRED)
include(${CMAKE_SOURCE_DIR}/cmake/Modules/systemdservice.cmake)
include_directories(SYSTEM ${MYSQLPP_INCLUDE_DIRS} ${JSONCPP_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR} ${LUA_INCLUDE_DIR} ${ASIO_INCLUDE_DIRS})

####
## Sources
#
file (GLOB	   HEADER_FILES "include/*.h")
file (GLOB    BACK_SOURCE_FILES "src/*.cpp")
file (GLOB    LUA_ALERTER_FILES "luaAlerters/*.lua" )
file (GLOB    CPP_ALERTER_FILES "alerters/*.cpp" )

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/sws)
install(FILES ${HEADER_FILES} DESTINATION include/watched)
configure_file ("${PROJECT_SOURCE_DIR}/include/config.h.in"
                "${PROJECT_BINARY_DIR}/include/config.h" )
configure_file ("${PROJECT_SOURCE_DIR}/tools/conf/back.config.json.in"
                "${PROJECT_BINARY_DIR}/back.config.json" )
include_directories(${PROJECT_BINARY_DIR}/include)

if (SYSTEMD_FOUND)
	configure_file(
		"${PROJECT_SOURCE_DIR}/tools/systemd/watched.back.service.in"
		"${PROJECT_BINARY_DIR}/tools/systemd/watched.back.service"
		@ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tools/systemd/watched.back.service"
		DESTINATION "${SYSTEMD_SERVICES_INSTALL_DIR}")
	if (EXISTS /etc/logrotate.d)
		configure_file(
			"${PROJECT_SOURCE_DIR}/tools/logrotate/watched.back.in"
			"${PROJECT_BINARY_DIR}/tools/logrotate/watched.back"
			@ONLY)
		install(FILES "${CMAKE_CURRENT_BINARY_DIR}/tools/logrotate/watched.back"
			DESTINATION "/etc/logrotate.d")
	endif (EXISTS /etc/logrotate.d)
endif (SYSTEMD_FOUND)
####
## Targets
#

# Alerters plugins
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/alerters)
foreach( alerter_cpp ${CPP_ALERTER_FILES} )
	get_filename_component( alerter ${alerter_cpp} NAME_WE )
	add_library( ${alerter} SHARED ${alerter_cpp} )
	install(TARGETS ${alerter} DESTINATION lib/watched/alerter.cpp)
endforeach( alerter_cpp ${CPP_ALERTER_FILES} )

add_executable(watched.back ${BACK_SOURCE_FILES} ${PROJECT_BINARY_DIR}/back.config.json)
target_link_libraries(watched.back dl ${MYSQLPP_LIBRARIES} ${JSONCPP_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${LUA_LIBRARIES})
install(TARGETS watched.back DESTINATION sbin COMPONENT binaries)
install(FILES ${PROJECT_BINARY_DIR}/back.config.json DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/watched COMPONENT config)

foreach( alerter_lua ${LUA_ALERTER_FILES} )
	install(FILES ${alerter_lua} DESTINATION lib/watched/alerter.lua)
endforeach( alerter_lua ${LUA_ALERTER_FILES} )


####
## Usefull debug stuff
#
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

