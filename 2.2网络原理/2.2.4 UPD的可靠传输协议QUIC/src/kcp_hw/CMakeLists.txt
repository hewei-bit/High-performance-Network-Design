# 单个目录实现
# CMake 最低版本号要求
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# 设置release版本还是debug版本
if(${CMAKE_BUILD_TYPE} MATCHES "Release")
    MESSAGE(STATUS "Release版本")
    SET(BuildType "Release")
else()
    SET(BuildType "Debug")
    MESSAGE(STATUS "Debug版本")
endif()

#设置lib库目录
SET(RELEASE_DIR ${PROJECT_SOURCE_DIR}/release)

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

# 添加子目录
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/delay")
ADD_SUBDIRECTORY("${CMAKE_CURRENT_SOURCE_DIR}/kcp")

install(FILES ikcp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# 将执行文件安装到LIB目录
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
    add_executable(client ./client/client.c)
    target_link_libraries(client pthread kcp delay)

    # server
    add_executable(server ./server/server.c)
    target_link_libraries(server kcp)
    # test
    add_executable(kcp_test ./test/kcp_test.cpp)
    target_link_libraries(kcp_test kcp)

    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        target_compile_options(kcp_test PRIVATE /utf-8)
    endif()

endif ()
