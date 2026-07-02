function(solar_net_add_test TEST_NAME)
  add_executable(${TEST_NAME} ${ARGN})
  target_link_libraries(${TEST_NAME}
    PRIVATE
      solar_net
      GTest::gtest_main
      GTest::gmock
  )
  target_include_directories(${TEST_NAME}
    PRIVATE
      ${SOLAR_NET_PUBLIC_DIR}
  )
  gtest_discover_tests(${TEST_NAME})
endfunction()

solar_net_add_test(test_version test_version.cpp)
solar_net_add_test(test_time test_time.cpp)
solar_net_add_test(test_logger test_logger.cpp)
solar_net_add_test(test_buffer test_buffer.cpp)
solar_net_add_test(test_thread test_thread.cpp)
solar_net_add_test(test_thread_pool test_thread_pool.cpp)
solar_net_add_test(test_channel test_channel.cpp)
solar_net_add_test(test_poller test_poller.cpp)
solar_net_add_test(test_event_loop test_event_loop.cpp)
solar_net_add_test(test_timer_queue test_timer_queue.cpp)
