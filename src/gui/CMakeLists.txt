qt_add_executable(it-pixel-gui
    main.cpp
    MainWindow.cpp
    MainWindow.hpp
    EngineWorker.cpp
    EngineWorker.hpp)
set_target_properties(it-pixel-gui PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON)
target_include_directories(it-pixel-gui PRIVATE ../core)
target_link_libraries(it-pixel-gui PRIVATE Qt6::Widgets itpixel::core)
itpixel_set_warnings(it-pixel-gui)
