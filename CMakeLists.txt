cmake_minimum_required(VERSION 3.20)
project(tcp_test)

set(CMAKE_CXX_STANDARD 11)

include_directories(include)

set(SOURCES
    src/main.cc
    src/client.cc
    src/server.cc
    src/conn.cc
)

add_executable(tcp_test ${SOURCES})