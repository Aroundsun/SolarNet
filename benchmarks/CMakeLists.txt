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
