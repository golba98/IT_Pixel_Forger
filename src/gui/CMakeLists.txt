# GUI logic lives in a reusable static library so the smoke test can link
# against the real widgets and worker.
add_library(itpixel_gui STATIC
    MainWindow.cpp
    MainWindow.hpp
    EngineWorker.cpp
    EngineWorker.hpp
    Theme.cpp
    Theme.hpp)
target_include_directories(itpixel_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../core)
target_link_libraries(itpixel_gui PUBLIC Qt6::Widgets itpixel::core)
itpixel_set_warnings(itpixel_gui)

add_executable(it-pixel-gui main.cpp)
set_target_properties(it-pixel-gui PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON)
target_link_libraries(it-pixel-gui PRIVATE itpixel_gui)
itpixel_set_warnings(it-pixel-gui)
