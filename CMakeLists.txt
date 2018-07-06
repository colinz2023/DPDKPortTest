cmake_minimum_required(VERSION 3.10)
project(dpdk_port_test)

set(CMAKE_CXX_STANDARD 11)

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ./build)

include_directories(dpdk/local/include)
include_directories(dpdk/local/include/dpdk)
link_directories(dpdk/local/lib)

#set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build)

add_subdirectory(src)
