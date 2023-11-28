cmake_minimum_required(VERSION 3.6)

project(replication_checker)
include_directories("D:/code/postgres/postgresql-15.3-4-windows-x64-binaries/pgsql/include")
add_library( pq SHARED IMPORTED )
add_library( ssl SHARED IMPORTED )
add_library( crypto SHARED IMPORTED )

set_target_properties( pq PROPERTIES 
IMPORTED_LOCATION_DEBUG "D:/code/postgres/postgresql-15.3-4-windows-x64-binaries/pgsql/lib/libpq.dll" 
IMPORTED_IMPLIB_DEBUG "D:/code/postgres/postgresql-15.3-4-windows-x64-binaries/pgsql/lib/libpq.lib"
IMPORTED_LOCATION "D:/code/postgres/postgresql-15.3-4-windows-x64-binaries/pgsql/lib/libpq.dll" 
IMPORTED_IMPLIB "D:/code/postgres/postgresql-15.3-4-windows-x64-binaries/pgsql/lib/libpq.lib")

set_target_properties( ssl PROPERTIES 
IMPORTED_LOCATION_DEBUG "D:/openssl/openssl-3/x64/bin/libssl-3-x64.dll" 
IMPORTED_IMPLIB_DEBUG "D:/openssl/openssl-3/x64/lib/libssl.lib"
IMPORTED_LOCATION "D:/openssl/openssl-3/x64/bin/libssl-3-x64.dll" 
IMPORTED_IMPLIB "D:/openssl/openssl-3/x64/lib/libssl.lib")


set_target_properties( crypto PROPERTIES 
IMPORTED_LOCATION_DEBUG "D:/openssl/openssl-3/x64/bin/crypto-3-x64.dll" 
IMPORTED_IMPLIB_DEBUG "D:/openssl/openssl-3/x64/lib/crypto.lib"
IMPORTED_LOCATION "D:/openssl/openssl-3/x64/bin/crypto-3-x64.dll" 
IMPORTED_IMPLIB "D:/openssl/openssl-3/x64/lib/crypto.lib")


link_directories("D:/code/postgres/postgresql-15.3-4-windows-x64-binaries/pgsql/lib")
add_executable(replication_checker test.cpp util.h checker_postgres_server.h)
target_link_libraries(replication_checker PUBLIC  pq)
set_property(TARGET replication_checker PROPERTY CXX_STANDARD 23)
