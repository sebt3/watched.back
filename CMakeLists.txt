cmake_minimum_required (VERSION 2.8.7)
project (watched.back)

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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

####
## Dependencies
#
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Threads REQUIRED)
find_package(Boost 1.53.0 COMPONENTS regex system thread filesystem date_time REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Jsoncpp REQUIRED)
find_package(Mysqlpp REQUIRED)
include_directories(SYSTEM ${MYSQLPP_INCLUDE_DIRS} ${JSONCPP_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${OPENSSL_INCLUDE_DIR})

####
## Sources
#
file (GLOB	   HEADER_FILES "include/*.h")
file (GLOB    BACK_SOURCE_FILES "src/*.cpp")

include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/sws)
install(FILES ${HEADER_FILES} DESTINATION include/watched)

####
## Targets
#
add_executable(watched.back ${BACK_SOURCE_FILES})
target_link_libraries(watched.back ${MYSQLPP_LIBRARIES} ${JSONCPP_LIBRARIES} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
install(FILES watched.back DESTINATION sbin)

####
## Usefull debug stuff
#
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

