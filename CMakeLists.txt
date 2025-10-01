cmake_minimum_required(VERSION 3.10)
project(LowLatencyChat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -pthread")

add_executable(low_latency_server src/server.cpp)
target_link_libraries(low_latency_server PRIVATE pthread)

add_executable(low_latency_client src/client.cpp)
target_link_libraries(low_latency_client PRIVATE pthread)