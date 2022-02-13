cmake_minimum_required(VERSION 3.0)

project(websocket_server_HW)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_BUILD_TYPE Debug)

include_directories(${CMAKE_SOURCE_DIR}/inc)

add_executable(websocket_server_v1 src/example/websocket_server_v1.c)
TARGET_LINK_LIBRARIES(websocket_server_v1 ssl)
TARGET_LINK_LIBRARIES(websocket_server_v1 crypto)

add_executable(websocket_server src/example/websocket_server.c)
TARGET_LINK_LIBRARIES(websocket_server ssl)
TARGET_LINK_LIBRARIES(websocket_server crypto)
