cmake_minimum_required(VERSION 3.0)

project(tcpserver_HW)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_BUILD_TYPE Debug)

include_directories(${CMAKE_SOURCE_DIR}/inc)

add_executable(tcpserver_HW src/practice/tcpserver_HW.c)

add_executable(tcpserver src/example/tcpserver.c)