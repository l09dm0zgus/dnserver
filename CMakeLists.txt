cmake_minimum_required(VERSION 3.11)
project(DNSServer C)

set(CMAKE_C_STANDARD 11)
add_subdirectory(libs/json-c)
find_package(json-c CONFIG)

add_executable(DNSServer main.c
        ThreadPool.h
        ThreadPool.c
        DNSServer.h
        DNSServer.c)
target_include_directories(DNSServer PRIVATE libs)
target_link_libraries(DNSServer PRIVATE json-c)