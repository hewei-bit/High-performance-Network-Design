# 单个目录实现
# CMake 最低版本号要求
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# 工程
project(kcp_hw LANGUAGES C)

# set
SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O0 -Wall")

# 添加头文件路径
include(CTest)
include(GNUInstallDirs)
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/client")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/delay")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/kcp")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/server")
INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}/test")

# 加载所有的源码，和makefile wildcard类似
AUX_SOURCE_DIRECTORY(./client ./server ./delay ./kcp ./test DIR_SRCS)

# 默认是静态库
add_library(kcp STATIC ikcp.c)

install(FILES ikcp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(TARGETS kcp
    EXPORT kcp-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
)

# 生成执行文件
if (BUILD_TESTING)

    enable_language(CXX)
    # client
    add_executable(client client.c ikcp.c delay.c)
    target_link_libraries(client pthread)
    # server
    add_executable(server server.c ikcp.c)
    # test_kcp
    add_executable(test_kcp test.cpp ikcp.c)

    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        target_compile_options(kcp_test PRIVATE /utf-8)
    endif()

endif ()
