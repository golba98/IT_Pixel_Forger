#pragma once

#include <cstdint>
#include <limits>

namespace itpixel {

// Transformation settings shared by the GUI, the CLI and the tests.
// All values are validated and clamped by validated(); construction from
// raw user input goes through validated() rather than the defaults.
struct MosaicSettings {
    // Edge length of one mosaic block in input-image pixels. The effective
    // value may be raised for extremely large images (see MosaicEngine).
    int blockSize{1};

    // Number of animation batches used to reveal the mosaic.
    int animationSteps{120};

    // 0.0 keeps the sampled tile untouched, 1.0 replaces it entirely with
    // the reference target colour. This mirrors the original engine's
    // behaviour: blend 1.0 yields a flat target-coloured block.
    double blendStrength{1.0};

    // Seed for the tile-selection jitter. Fixed seed => reproducible output.
    std::uint32_t seed{12345};

    // Largest block count the engine accepts before raising the block size.
    // Chosen so that per-block bookkeeping stays well under 1 GiB even for
    // 8K inputs. The original Python code attempted a similar correction but
    // its 60M-pixel threshold made it a no-op for nearly all images.
    static constexpr std::size_t kMaxWorkingBlocks = 8'000'000;

    [[nodiscard]] static MosaicSettings validated(MosaicSettings settings);
};

} // namespace itpixel
