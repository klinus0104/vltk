cmake_minimum_required(VERSION 3.16)
project(sword3paysys_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(sword3paysys
    src/main.cpp
    src/account_store.cpp
)

target_compile_options(sword3paysys PRIVATE -Wall -Wextra -Wpedantic)

find_path(FREETDS_INCLUDE_DIR sybdb.h)
find_library(FREETDS_LIBRARY sybdb)

if(FREETDS_INCLUDE_DIR AND FREETDS_LIBRARY)
    target_include_directories(sword3paysys PRIVATE ${FREETDS_INCLUDE_DIR})
    target_link_libraries(sword3paysys PRIVATE ${FREETDS_LIBRARY})
    target_compile_definitions(sword3paysys PRIVATE HAVE_FREETDS=1)
    message(STATUS "FreeTDS DB-Lib enabled: ${FREETDS_LIBRARY}")
else()
    message(FATAL_ERROR "FreeTDS DB-Lib is required for the MSSQL account backend")
endif()
