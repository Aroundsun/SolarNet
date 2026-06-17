include(FetchContent)

FetchContent_Declare(
    spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

add_library(solar_net_log STATIC
    log.cpp
)
target_include_directories(solar_net_log PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(solar_net_log PUBLIC spdlog::spdlog)

add_executable(test_log test_log.cpp)
set_target_properties(test_log PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
target_link_libraries(test_log PRIVATE solar_net_log GTest::gtest_main)
add_test(NAME test_log COMMAND ${CMAKE_BINARY_DIR}/bin/test_log)
