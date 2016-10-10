#find_package(PkgConfig)
#pkg_check_modules(PC_LIBMYSQLPP QUIET mysqlpp)
#set(MYSQLPP_DEFINITIONS ${PC_LIBMYSQLPP_CFLAGS_OTHER})

find_path(MYSQL_INCLUDE_DIR mysql_version.h
          HINTS /usr/include/mysql /usr/local/include/mysql )

find_path(MYSQLPP_INCLUDE_DIR mysql++/mysql++.h
          HINTS /usr/include /usr/local/include )

find_library(MYSQLPP_LIBRARY NAMES mysqlpp libmysqlpp
             HINTS /usr/lib /usr/local/lib )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibMYSQLPP  DEFAULT_MSG
                                  MYSQLPP_LIBRARY MYSQLPP_INCLUDE_DIR)

mark_as_advanced(MYSQLPP_INCLUDE_DIR MYSQLPP_LIBRARY )

set(MYSQLPP_LIBRARIES ${MYSQLPP_LIBRARY} )
set(MYSQLPP_INCLUDE_DIRS ${MYSQLPP_INCLUDE_DIR} ${MYSQL_INCLUDE_DIR} )
