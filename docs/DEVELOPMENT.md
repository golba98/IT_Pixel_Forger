# Development guide

## Workflow

```bash
# configure + build + test (Fedora; see docs/BUILDING.md for other platforms)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# a debug build with a compile_commands.json for tooling
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-debug --parallel
```

The GUI smoke test runs under `QT_QPA_PLATFORM=offscreen`, so no display is
needed anywhere in the pipeline.

## Testing

Tests live in `tests/` and use vendored
[doctest](https://github.com/doctest/doctest) (MIT, `third_party/doctest`).
Two executables:

- `itpixel_tests` — engine correctness (block means, luminance matching,
  blend endpoints, determinism, cancellation, progress, 1x1 and odd-sized
  images), GIF validity (decoded with giflib itself), and CLI argument
  parsing.
- `itpixel_gui_smoke` — drives the real `MainWindow` end-to-end offscreen:
  transformation through the UI, control state, preview, and worker
  cancellation.

Run a single binary directly for doctest's reporting:

```bash
./build/tests/itpixel_tests
QT_QPA_PLATFORM=offscreen ./build/tests/itpixel_gui_smoke
```

When adding engine behaviour, prefer a deterministic test: fixed inputs and
a fixed seed make assertions exact.

## Formatting and static analysis

- `clang-format` enforces the style (`.clang-format`). Format everything:

  ```bash
  find include src tests -name '*.hpp' -o -name '*.cpp' | xargs clang-format -i
  ```

- `clang-tidy` config lives in `.clang-tidy`; run it against the debug build:

  ```bash
  clang-tidy -p build-debug src/core/*.cpp src/cli/*.cpp src/gui/*.cpp
  ```

  CI runs the same command on every push and pull request.

## Conventions

- Naming: `CamelCase` for types, `camelBack` for functions and variables,
  trailing `_` for private data members, `k` prefix for constants
  (enforced by clang-tidy).
- Errors are reported as values (`std::optional` + error string) for
  expected runtime failures; exceptions are only for programming errors
  (null images handed to the engine).
- Image data is `QImage` in `Format_RGB888`; pixel access goes through
  `scanLine()` with bounds-safe offsets, never through `QImage::setPixel`
  in hot loops.
- No raw owning pointers: `GifExporter` is a move-only RAII type over a
  giflib handle; everything else is values or Qt parents.
- Comments explain *why* (behaviour parity with the original engine,
  giflib quirks), not *what*.

## Repository layout

```text
assets/reference/   default reference image (it.jpeg)
assets/samples/      tester image (procedurally generated, rights-free)
cmake/               shared CMake helpers
docs/                developer documentation and README assets
include/itpixel/     public core-library headers
src/core/            engine, GIF exporter, path helpers
src/cli/             command line application
src/gui/             desktop application (widgets, worker, theme)
tests/               doctest suite + headless GUI smoke test
third_party/         vendored giflib and doctest (both MIT)
```

## Benchmarks

To measure a change, compare against a fixed workload, e.g.:

```bash
/usr/bin/time -f "%es" ./build/it-pixel-cli input.png --output /tmp/out.gif \
    --reference assets/reference/it.jpeg --steps 120 --block-size 1 --seed 42
```

Document any claimed improvement with the exact command, machine and
numbers — see the README for the current baseline measurements.
