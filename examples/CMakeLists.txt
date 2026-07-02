function(solar_net_add_example EXAMPLE_NAME)
  add_executable(${EXAMPLE_NAME} ${ARGN})
  target_link_libraries(${EXAMPLE_NAME}
    PRIVATE
      solar_net
  )
  target_include_directories(${EXAMPLE_NAME}
    PRIVATE
      ${SOLAR_NET_PUBLIC_DIR}
  )
endfunction()

solar_net_add_example(example_version example_version.cpp)
solar_net_add_example(example_logger example_logger.cpp)
solar_net_add_example(example_buffer example_buffer.cpp)
solar_net_add_example(example_thread_pool example_thread_pool.cpp)
solar_net_add_example(example_channel example_channel.cpp)
solar_net_add_example(example_poller example_poller.cpp)
solar_net_add_example(example_event_loop example_event_loop.cpp)
solar_net_add_example(example_timer_queue example_timer_queue.cpp)
solar_net_add_example(example_event_loop_thread example_event_loop_thread.cpp)
solar_net_add_example(example_event_loop_thread_pool example_event_loop_thread_pool.cpp)
solar_net_add_example(example_inet_address example_inet_address.cpp)
solar_net_add_example(example_socket example_socket.cpp)
solar_net_add_example(example_acceptor example_acceptor.cpp)
