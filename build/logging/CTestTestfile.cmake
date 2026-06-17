# CMake generated Testfile for 
# Source directory: /home/xhy/workplace/SolarNet/logging
# Build directory: /home/xhy/workplace/SolarNet/build/logging
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(test_log "/home/xhy/workplace/SolarNet/build/bin/test_log")
set_tests_properties(test_log PROPERTIES  _BACKTRACE_TRIPLES "/home/xhy/workplace/SolarNet/logging/CMakeLists.txt;26;add_test;/home/xhy/workplace/SolarNet/logging/CMakeLists.txt;0;")
subdirs("../_deps/spdlog-build")
