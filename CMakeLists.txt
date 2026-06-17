cmake_minimum_required(VERSION 3.14)
project(SolarNet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(solar_net STATIC
    src/acceptor.cpp
    src/channel.cpp
    src/event_loop.cpp
    src/event_loop_thread.cpp
    src/event_loop_thread_pool.cpp
    src/epoll_poller.cpp
    src/tcp_connection.cpp
    src/tcp_server.cpp
)
target_include_directories(solar_net PUBLIC ${CMAKE_SOURCE_DIR}/src)

enable_testing()

function(add_solar_net_test name)
    add_executable(${name} tests/${name}.cpp)
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/tests)
    target_link_libraries(${name} PRIVATE solar_net GTest::gtest_main)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_solar_net_test(test_buffer)
add_solar_net_test(test_socket)
add_solar_net_test(test_channel)
add_solar_net_test(test_event_loop)
add_solar_net_test(test_tcp_connection)
add_solar_net_test(test_acceptor)
add_solar_net_test(test_event_loop_thread)
add_solar_net_test(test_event_loop_thread_pool)
add_solar_net_test(test_tcp_server)
