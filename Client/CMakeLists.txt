cmake_minimum_required(VERSION 3.10)
project(dhcp_client)

set(CMAKE_CXX_STANDARD 11)

add_executable(dhcp_client main.cpp simpletun.cpp)
target_link_libraries(dhcp_client pthread)