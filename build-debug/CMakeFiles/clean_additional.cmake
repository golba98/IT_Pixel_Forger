# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "src/gui/CMakeFiles/it-pixel-gui_autogen.dir/AutogenUsed.txt"
  "src/gui/CMakeFiles/it-pixel-gui_autogen.dir/ParseCache.txt"
  "src/gui/CMakeFiles/itpixel_gui_autogen.dir/AutogenUsed.txt"
  "src/gui/CMakeFiles/itpixel_gui_autogen.dir/ParseCache.txt"
  "src/gui/it-pixel-gui_autogen"
  "src/gui/itpixel_gui_autogen"
  "tests/CMakeFiles/itpixel_gui_smoke_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/itpixel_gui_smoke_autogen.dir/ParseCache.txt"
  "tests/CMakeFiles/itpixel_tests_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/itpixel_tests_autogen.dir/ParseCache.txt"
  "tests/itpixel_gui_smoke_autogen"
  "tests/itpixel_tests_autogen"
  )
endif()
