# Building IT Pixel Forger

The project uses CMake (3.25+) with the C++23 language standard.

## Requirements

| Tool / library | Version |
|---|---|
| CMake | 3.25 or newer |
| Ninja (recommended) or another generator | — |
| C++ compiler | GCC 12+, Clang 16+, or MSVC 2022 (recent enough for C++23) |
| Qt 6 | 6.2 or newer, `QtGui` + `QtWidgets` (`qt6-base`) |

No other third-party dependencies: giflib (GIF encoding/decoding) and
doctest (tests) are vendored under `third_party/` with their MIT licenses.

## Fedora Linux

```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel
```

Configure, build and test:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The binaries are `build/src/gui/it-pixel-gui` and `build/it-pixel-cli`.
The GUI smoke test runs headlessly via `QT_QPA_PLATFORM=offscreen`, which
CTest sets automatically.

## Ubuntu / Debian

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev libgl1-mesa-dev
```

Then use the same CMake commands as above.

## Windows (MSVC)

1. Install Visual Studio 2022 with the "Desktop development with C++"
   workload and CMake (bundled with Visual Studio or standalone).
2. Install Qt 6 for MSVC 2022 (x64), for example with the
   [Qt online installer](https://www.qt.io/download-open-source) or
   [aqtinstall](https://github.com/miurahr/aqtinstall).

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Set `CMAKE_PREFIX_PATH` to your Qt installation. The GitHub Actions
workflow (`.github/workflows/ci.yml`) shows a fully scripted Windows
build using the Qt online packages.

## Build options

| Option | Default | Description |
|---|---|---|
| `ITPIXEL_BUILD_GUI` | `ON` | Build the Qt Widgets desktop application |
| `ITPIXEL_BUILD_TESTS` | `ON` | Build the test suite |
| `ITPIXEL_WARNINGS_AS_ERRORS` | `OFF` | Turn compiler warnings into errors |

## Installing

```bash
cmake --install build
```

Installs `it-pixel-cli` and `it-pixel-gui` to `bin/` and the default
reference image to `share/itpixel/`. Run `cmake --install build --prefix
<dir>` for a relocatable install.

## Troubleshooting

- **`Qt6 not found`**: make sure `Qt6Core`/`Qt6Gui`/`Qt6Widgets` CMake
  packages are installed (`qt6-qtbase-devel` on Fedora, `qt6-base-dev` on
  Ubuntu) and set `CMAKE_PREFIX_PATH` to the Qt prefix when needed.
- **GUI tests fail on a headless machine**: they should not — CTest sets
  `QT_QPA_PLATFORM=offscreen` for the smoke test. If you run the binary
  manually, export `QT_QPA_PLATFORM=offscreen`.
- **`std::filesystem`/C++23 errors**: your compiler is too old. The
  project needs GCC 12+, Clang 16+ or a current MSVC.
