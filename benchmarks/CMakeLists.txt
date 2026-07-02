function(solar_net_add_benchmark BENCH_NAME)
  add_executable(${BENCH_NAME} ${ARGN})
  target_link_libraries(${BENCH_NAME}
    PRIVATE
      solar_net
      benchmark::benchmark
  )
  target_include_directories(${BENCH_NAME}
    PRIVATE
      ${SOLAR_NET_PUBLIC_DIR}
  )
endfunction()

solar_net_add_benchmark(bench_version bench_version.cpp)
solar_net_add_benchmark(bench_buffer bench_buffer.cpp)
solar_net_add_benchmark(bench_thread_pool bench_thread_pool.cpp)
solar_net_add_benchmark(bench_channel bench_channel.cpp)
solar_net_add_benchmark(bench_poller bench_poller.cpp)
solar_net_add_benchmark(bench_event_loop bench_event_loop.cpp)
solar_net_add_benchmark(bench_timer_queue bench_timer_queue.cpp)
solar_net_add_benchmark(bench_event_loop_thread bench_event_loop_thread.cpp)
solar_net_add_benchmark(bench_event_loop_thread_pool bench_event_loop_thread_pool.cpp)
solar_net_add_benchmark(bench_inet_address bench_inet_address.cpp)
solar_net_add_benchmark(bench_socket bench_socket.cpp)
solar_net_add_benchmark(bench_acceptor bench_acceptor.cpp)
solar_net_add_benchmark(bench_tcp_connection bench_tcp_connection.cpp)
solar_net_add_benchmark(bench_tcp_server bench_tcp_server.cpp)
