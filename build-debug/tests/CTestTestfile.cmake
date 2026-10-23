# CMake generated Testfile for 
# Source directory: /home/k9-vortex/Development/5-C++/4-IT Pixel Generator/tests
# Build directory: /home/k9-vortex/Development/5-C++/4-IT Pixel Generator/build-debug/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[itpixel_tests]=] "/home/k9-vortex/Development/5-C++/4-IT Pixel Generator/build-debug/tests/itpixel_tests")
set_tests_properties([=[itpixel_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/k9-vortex/Development/5-C++/4-IT Pixel Generator/tests/CMakeLists.txt;21;add_test;/home/k9-vortex/Development/5-C++/4-IT Pixel Generator/tests/CMakeLists.txt;0;")
add_test([=[itpixel_gui_smoke]=] "/home/k9-vortex/Development/5-C++/4-IT Pixel Generator/build-debug/tests/itpixel_gui_smoke")
set_tests_properties([=[itpixel_gui_smoke]=] PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/k9-vortex/Development/5-C++/4-IT Pixel Generator/tests/CMakeLists.txt;30;add_test;/home/k9-vortex/Development/5-C++/4-IT Pixel Generator/tests/CMakeLists.txt;0;")
