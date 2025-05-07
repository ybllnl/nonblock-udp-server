cmake_minimum_required(VERSION 3.10)
project(NonBlockingUDPServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server src/server.cpp)
add_executable(client src/client.cpp)

find_package(Threads REQUIRED)

target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client PRIVATE Threads::Threads)

