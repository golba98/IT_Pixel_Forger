#pragma once

#include <cstdint>

namespace itpixel {

// One extracted sample block of the input image, used as mosaic material.
struct Tile {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    double luminance{};
    int x{}; // top-left corner in input-image coordinates
    int y{};
    int w{};
    int h{};
};

} // namespace itpixel
