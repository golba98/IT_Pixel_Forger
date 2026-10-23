add_executable(itpixel_tests
    test_main.cpp
    engine_tests.cpp
    gif_tests.cpp
    cli_tests.cpp
    ../src/cli/CliOptions.cpp)
target_compile_features(itpixel_tests PRIVATE cxx_std_23)
target_include_directories(itpixel_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/cli
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/doctest)
target_link_libraries(itpixel_tests
    PRIVATE
        itpixel::core
        giflib)
target_compile_definitions(itpixel_tests PRIVATE
    ITPIXEL_REFERENCE_ASSET="${CMAKE_CURRENT_SOURCE_DIR}/../assets/reference/it.jpeg")
itpixel_set_warnings(itpixel_tests)

add_test(NAME itpixel_tests COMMAND itpixel_tests)

# Headless GUI smoke test against the real widgets and worker thread.
if(ITPIXEL_BUILD_GUI)
    add_executable(itpixel_gui_smoke gui_smoke.cpp)
    target_compile_features(itpixel_gui_smoke PRIVATE cxx_std_23)
    target_include_directories(itpixel_gui_smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/gui)
    target_link_libraries(itpixel_gui_smoke PRIVATE itpixel_gui)
    itpixel_set_warnings(itpixel_gui_smoke)
    add_test(NAME itpixel_gui_smoke COMMAND itpixel_gui_smoke)
    set_tests_properties(itpixel_gui_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
