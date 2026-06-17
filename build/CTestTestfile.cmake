# CMake generated Testfile for 
# Source directory: /home/xhy/workplace/SolarNet
# Build directory: /home/xhy/workplace/SolarNet/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_buffer "/home/xhy/workplace/SolarNet/build/test_buffer")
set_tests_properties(test_buffer PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;37;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
add_test(test_socket "/home/xhy/workplace/SolarNet/build/test_socket")
set_tests_properties(test_socket PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;38;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
add_test(test_channel "/home/xhy/workplace/SolarNet/build/test_channel")
set_tests_properties(test_channel PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;39;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
add_test(test_event_loop "/home/xhy/workplace/SolarNet/build/test_event_loop")
set_tests_properties(test_event_loop PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;40;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
add_test(test_tcp_connection "/home/xhy/workplace/SolarNet/build/test_tcp_connection")
set_tests_properties(test_tcp_connection PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;41;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
add_test(test_acceptor "/home/xhy/workplace/SolarNet/build/test_acceptor")
set_tests_properties(test_acceptor PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;42;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
add_test(test_event_loop_thread "/home/xhy/workplace/SolarNet/build/test_event_loop_thread")
set_tests_properties(test_event_loop_thread PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;43;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
add_test(test_event_loop_thread_pool "/home/xhy/workplace/SolarNet/build/test_event_loop_thread_pool")
set_tests_properties(test_event_loop_thread_pool PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;44;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
add_test(test_tcp_server "/home/xhy/workplace/SolarNet/build/test_tcp_server")
set_tests_properties(test_tcp_server PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/CMakeLists.txt;34;add_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;45;add_solar_net_test;/home/xhy/workplace/SolarNet/CMakeLists.txt;0;")
subdirs("_deps/googletest-build")
