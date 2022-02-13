cmake_minimum_required(VERSION 3.0)

project(reactor_singlecb_HW)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_BUILD_TYPE Debug)

include_directories(${CMAKE_SOURCE_DIR}/inc)

add_executable(reactor_singlecb_hw src//HW/reactor_singlecb_hw.c)

add_executable(reactor_singlecb src/example/reactor_singlecb.c)