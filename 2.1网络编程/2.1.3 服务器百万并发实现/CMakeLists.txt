cmake_minimum_required(VERSION 3.0)

project(reactor_singlecb_C1000K_HW)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_BUILD_TYPE Debug)

include_directories(${CMAKE_SOURCE_DIR}/inc)

add_executable(reactor_singlecb_hw_C1000K src//practice/reactor_singlecb_hw_C1000K.c)

add_executable(mul_port_client_epoll src/example/mul_port_client_epoll.c)
add_executable(reactor_mulcb src/example/reactor_mulcb.c)
add_executable(reactor_singlecb src/example/reactor_singlecb.c)