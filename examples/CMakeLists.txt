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
