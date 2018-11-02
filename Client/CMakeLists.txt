cmake_minimum_required(VERSION 3.10)
project(tvpn_client)

set(CMAKE_CXX_STANDARD 11)

add_executable(tvpn_client main.cpp simpletun.cpp)
target_link_libraries(tvpn_client pthread)