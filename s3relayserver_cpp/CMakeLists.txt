cmake_minimum_required(VERSION 3.16)
project(s3relayserver_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(s3relayserver
    src/main.cpp
    src/relay_store.cpp
)

target_compile_options(s3relayserver PRIVATE -Wall -Wextra -Wpedantic)

find_path(FREETDS_INCLUDE_DIR sybdb.h)
find_library(FREETDS_LIBRARY sybdb)

if(FREETDS_INCLUDE_DIR AND FREETDS_LIBRARY)
    target_include_directories(s3relayserver PRIVATE ${FREETDS_INCLUDE_DIR})
    target_link_libraries(s3relayserver PRIVATE ${FREETDS_LIBRARY})
    target_compile_definitions(s3relayserver PRIVATE HAVE_FREETDS=1)
    message(STATUS "FreeTDS DB-Lib enabled: ${FREETDS_LIBRARY}")
else()
    message(FATAL_ERROR "FreeTDS DB-Lib is required for the MSSQL relay backend")
endif()
