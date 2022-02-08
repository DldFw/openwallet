cmake_minimum_required(VERSION 2.8)

project(otc-tool)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_BUILD_TYPE "Debug")

include_directories(./include)

aux_source_directory(./src DIR_SRCS)

set(base_src src/main.cpp
			 src/common.cpp
			 )

set(link_lib curl mysqlclient event boost_system boost_filesystem boost_program_options glog gmp)

add_executable(otc-tool ${base_src} ${job_src})

target_link_libraries(otc-tool ${link_lib})
