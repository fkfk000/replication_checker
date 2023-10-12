cmake_minimum_required(VERSION 3.6)

project(replication_checker)
include_directories("C:/Code/15/include")
add_library( pq SHARED IMPORTED )
add_library( ssl SHARED IMPORTED )
add_library( crypto SHARED IMPORTED )

set_target_properties( pq PROPERTIES 
IMPORTED_LOCATION_DEBUG "C:/Code/15/lib/libpq.dll" 
IMPORTED_IMPLIB_DEBUG "C:/Code/15/lib/libpq.lib"
IMPORTED_LOCATION "C:/Code/15/lib/libpq.dll" 
IMPORTED_IMPLIB "C:/Code/15/lib/libpq.lib")

set_target_properties( ssl PROPERTIES 
IMPORTED_LOCATION_DEBUG "C:/Code/15/bin/libssl-3-x64.dll" 
IMPORTED_IMPLIB_DEBUG "C:/Code/15/bin/libssl.lib"
IMPORTED_LOCATION "C:/Code/15/bin/libssl-3-x64.dll" 
IMPORTED_IMPLIB "C:/Code/15/bin/libssl.lib")


set_target_properties( crypto PROPERTIES 
IMPORTED_LOCATION_DEBUG "C:/Code/15/bin/crypto-3-x64.dll" 
IMPORTED_IMPLIB_DEBUG "C:/Code/15/bin/crypto.lib"
IMPORTED_LOCATION "C:/Code/15/bin/crypto-3-x64.dll" 
IMPORTED_IMPLIB "C:/Code/15/bin/crypto.lib")


link_directories("C:/Code/15/lib")
add_executable(replication_checker test.cpp util.h checker_postgres_server.h)
target_link_libraries(replication_checker PUBLIC  pq)
set_property(TARGET replication_checker PROPERTY CXX_STANDARD 23)
