# Architecture

This document explains how IT Pixel Generator is structured, how the mosaic
transformation works, and where the thread boundaries are.

## Overview

The project is a small, focused native C++23 application with three build
targets:

```text
itpixel_core   static library: the mosaic engine and the GIF exporter
it-pixel-cli   command line tool: headless animated GIF generation
it-pixel-gui   Qt 6 Widgets desktop application
```

The engine is completely independent of the GUI; the CLI links only
`Qt6::Gui` (for `QImage`) and runs headless.

```text
                +---------------------+
 input image -->|                     |--> frames --> GUI preview (throttled)
 reference    ->|     MosaicEngine    |
                |  (itpixel_core)     |--> frames --> GifExporter --> .gif file
                +----------+----------+
                           ^  callbacks: status / progress / cancellation
                           |
              EngineWorker (GUI) or ConsoleCallbacks (CLI)
```

## The mosaic algorithm

The transformation rebuilds an input image out of blocks sampled from a
reference image. It reproduces the original Python engine's behaviour:

1. **Preparation.** The reference image is resized to the input size. The
   input is cut into a grid of `block_size` x `block_size` tiles (edge tiles
   are clamped). Each tile's average colour and luminance
   (`0.299 R + 0.587 G + 0.114 B`) are computed and the tiles are sorted by
   luminance (stable sort, as in Python).

2. **Target grid.** The same grid is applied to the reference image. Each
   block records its average colour, luminance and hue, and the blocks are
   sorted by hue — this spectral order is what drives the animation's
   "rainbow sweep" appearance.

3. **Animation.** The engine walks the hue-sorted blocks in
   `animation_steps` batches. For every block it finds, by binary search, the
   tile whose luminance best matches the block's target luminance, jitters
   the selection by `+/- tiles/200` positions (a seeded `std::mt19937`, so
   runs are reproducible), optionally blends the tile toward the target
   colour (`blend`), and paints it onto the canvas. A "shimmer" highlight
   (x1.4 brightness over the batch bounding box) is emitted with each batch.
   After all batches the engine yields ten padding frames plus the final
   plain canvas.

Scan previews (dimmed input, bright scanned rows, green scanline) are
emitted as frames during preparation, exactly like the original tool, and
are part of the exported animation.

### Intentional differences from the Python original

| Change | Why |
|---|---|
| True arithmetic mean per block (the original used `Pillow.resize((1,1))`, a bicubic filter, not an average) | Correct, testable block colour; identical for `block_size = 1` |
| Large-image policy caps the block **count** (8M) instead of using a 60M-pixel threshold that almost never triggered | Memory and runtime are driven by block count |
| Seeded, deterministic jitter (`--seed`) | Reproducible output; the original used unseeded global randomness |
| The CLI always writes the final frame even when `--frame-sample` skips it | The animation always ends on the completed mosaic |
| Qt `SmoothTransformation` instead of Pillow's LANCZOS for reference resizing | Qt-native; measured difference to the original engine is a mean of ~1.7/255 per channel on the final frame |
| Identical consecutive padding frames are kept (Pillow merged them) | Simpler, decoder-verified output |

## Threading model (GUI)

The engine is single-threaded by design; all heavy work happens on a worker
QThread:

- `EngineWorker` (a `QObject` moved to a persistent `QThread`) constructs
  and runs `MosaicEngine` off the GUI thread.
- Results reach the GUI exclusively through queued signals: status text,
  progress + elapsed + ETA, throttled preview images, the final image,
  cancellation and failure.
- Cancellation is an `std::atomic<bool>` checked by the engine between
  batches; the flag is reset when a job is submitted, so a cancel requested
  before a queued run still takes effect.
- Preview frames are coalesced (at most one every 50 ms) and downscaled to
  at most 1280 px in the worker thread, so the GUI event loop is never
  flooded.
- The worker thread is stopped on window close; the engine checks
  cancellation between every batch, so shutdown is prompt.

## GIF export

`GifExporter` wraps the vendored giflib (MIT, `third_party/giflib`) in a
RAII streaming writer:

- GIF89a with a NETSCAPE loop extension (loop forever by default).
- Per-frame graphics control extension carrying the delay
  (`round(100 / fps)` centiseconds).
- Each frame is quantized to an adaptive 256-colour palette (giflib's
  median-cut quantizer) with a local color table.
- Frames are written one at a time; nothing is accumulated in memory, so
  animation length is bounded only by disk space.

Generated files are verified with real decoders: the test suite decodes
them through giflib itself, and CI additionally decodes a CLI-produced GIF
with Pillow (`.github/scripts/verify_gif.py`).

## Error handling

- Image loading reports failures as user-facing messages (`std::optional`
  return + error string), never exceptions for bad files.
- The engine throws `std::invalid_argument` only for null images (a
  programming error); the worker converts exceptions into `failed`
  signals and the CLI converts them into exit code 1.
- CLI usage errors exit with code 2 and print `--help`.
