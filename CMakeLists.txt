cmake_minimum_required(VERSION 3.11)
project(DNSServer C)

set(CMAKE_C_STANDARD 11)

add_executable(DNSServer main.c
        ThreadPool.h
        ThreadPool.c)
