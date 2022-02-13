cmake_minimum_required(VERSION 3.0)

project(webserver_HW)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_BUILD_TYPE Debug)

include_directories(${CMAKE_SOURCE_DIR}/inc)

add_executable(webserver_hw src//practice/webserver_hw.c)

add_executable(webserver src/example/webserver.c)
