cmake_minimum_required(VERSION 3.10)
project(NonBlockingUDPServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    include
    ${CMAKE_BINARY_DIR}/include
)

add_executable(server src/server.cpp)
target_include_directories(server PRIVATE include)
add_executable(client src/client.cpp)
target_include_directories(client PRIVATE include)

option(NON_BLOCKING "Build non-blocking server/client" ON)

if(NON_BLOCKING)
    target_compile_definitions(server PRIVATE NON_BLOCKING)
    target_compile_definitions(client PRIVATE NON_BLOCKING)
endif()

set(EXCHANGE_IP "127.0.0.2" CACHE STRING "Default exchange IP for testing")
configure_file(
    "${CMAKE_SOURCE_DIR}/include/exchange_ip.h.in"
    "${CMAKE_BINARY_DIR}/include/exchange_ip.h"
)

option(TEST_LOCAL_NETWORK "Test local network" ON)

if(TEST_LOCAL_NETWORK)
    target_compile_definitions(server PRIVATE TEST_LOCAL_NETWORK)
    target_compile_definitions(client PRIVATE TEST_LOCAL_NETWORK)
endif()

find_package(Threads REQUIRED)

target_link_libraries(server PRIVATE Threads::Threads)
target_link_libraries(client PRIVATE Threads::Threads)

