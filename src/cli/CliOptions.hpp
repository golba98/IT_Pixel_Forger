#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "itpixel/MosaicSettings.hpp"

namespace itpixel::cli {

// Validated command line configuration. Parsing is kept free of image logic;
// it only normalizes and validates raw arguments.
struct CliOptions {
    std::filesystem::path input;
    std::filesystem::path output;    // resolved next to the input image by default
    std::filesystem::path reference; // empty = bundled default asset
    MosaicSettings settings;
    int fps{25};
    int frameSample{1};
    std::optional<std::uint32_t> seed; // nullopt = random per run

    // `args` excludes the program name. `error` is a user-facing message.
    static std::expected<CliOptions, std::string> parse(const std::vector<std::string>& args);

    static std::string helpText(const std::string& programName);
};

} // namespace itpixel::cli
