cmake_minimum_required(VERSION 3.10)

project(WeightRaft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(protobuf CONFIG REQUIRED)
find_package(unofficial-brpc CONFIG REQUIRED)
find_package(unofficial-braft CONFIG REQUIRED)
find_path(DBG_MACRO_INCLUDE_DIRS "dbg.h")

file(GLOB SRC_NODE CONFIGURE_DEPENDS src/*.cpp)


set(PROTO_FILE src/service.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})

include_directories(include ${CMAKE_CURRENT_BINARY_DIR} )

add_library(weight_raft SHARED ${PROTO_SRCS} ${SRC_NODE}) # 添加可执行文件
target_link_libraries(weight_raft unofficial::braft::braft-static)
target_link_libraries(weight_raft unofficial::brpc::brpc-static)

add_executable(node server_main.cpp)
target_link_libraries(node weight_raft)

add_executable(client client_main.cpp ${PROTO_SRCS})
target_link_libraries(client unofficial::brpc::brpc-static)