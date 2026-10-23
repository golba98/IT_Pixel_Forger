cmake_minimum_required(VERSION 3.25)

project(ITPixelGenerator
    VERSION 2.0.0
    DESCRIPTION "IT Pixel Generator - mosaic transformation engine, GUI and CLI"
    LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ITPIXEL_BUILD_GUI "Build the Qt Widgets desktop application" ON)
option(ITPIXEL_BUILD_TESTS "Build the test suite" ON)
option(ITPIXEL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 99)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Vendored giflib (MIT license, see third_party/giflib/COPYING)
# ---------------------------------------------------------------------------
add_library(giflib STATIC
    third_party/giflib/dgif_lib.c
    third_party/giflib/egif_lib.c
    third_party/giflib/gifalloc.c
    third_party/giflib/gif_err.c
    third_party/giflib/gif_hash.c
    third_party/giflib/quantize.c
    third_party/giflib/openbsd-reallocarray.c)
target_include_directories(giflib SYSTEM PUBLIC third_party/giflib)
target_compile_options(giflib PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-w>)
if(MSVC)
    target_compile_definitions(giflib PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ---------------------------------------------------------------------------
# Warning flags shared by project targets
# ---------------------------------------------------------------------------
include(cmake/CompilerWarnings.cmake)

# ---------------------------------------------------------------------------
# Core engine library
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Gui)

add_library(itpixel_core STATIC
    src/core/MosaicEngine.cpp
    src/core/GifExporter.cpp
    src/core/Paths.cpp)
add_library(itpixel::core ALIAS itpixel_core)
target_compile_features(itpixel_core PUBLIC cxx_std_23)
target_include_directories(itpixel_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
target_link_libraries(itpixel_core
    PUBLIC Qt6::Gui
    PRIVATE giflib)
itpixel_set_warnings(itpixel_core)

# ---------------------------------------------------------------------------
# Command line application
# ---------------------------------------------------------------------------
add_executable(it-pixel-cli
    src/cli/main.cpp
    src/cli/CliOptions.cpp)
target_include_directories(it-pixel-cli PRIVATE src/core)
target_link_libraries(it-pixel-cli PRIVATE itpixel::core)
itpixel_set_warnings(it-pixel-cli)

# ---------------------------------------------------------------------------
# Desktop application
# ---------------------------------------------------------------------------
if(ITPIXEL_BUILD_GUI)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    qt_standard_project_setup()
    add_subdirectory(src/gui)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ITPIXEL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS it-pixel-cli itpixel_core
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(ITPIXEL_BUILD_GUI)
    install(TARGETS it-pixel-gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES assets/reference/it.jpeg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/itpixel)
