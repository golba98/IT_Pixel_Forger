#include "CliOptions.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <numbers>

namespace itpixel::cli {
namespace {

constexpr int kMinSteps = 1;
constexpr int kMaxSteps = 10'000;
constexpr int kMinBlock = 1;
constexpr int kMaxBlock = 1'024;
constexpr int kMinFps = 1;
constexpr int kMaxFps = 50;

bool parseInteger(const std::string& text, long long& out) {
    const char* begin = text.c_str();
    const char* end = begin + text.size();
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

bool parseDouble(const std::string& text, double& out) {
    try {
        std::size_t consumed = 0;
        out = std::stod(text, &consumed);
        return consumed == text.size() && std::isfinite(out);
    } catch (...) {
        return false;
    }
}

} // namespace

std::string CliOptions::helpText(const std::string& programName) {
    return "IT Pixel Forger CLI - mosaic transformation with animated GIF export\n"
           "\n"
           "Usage:\n"
           "  " +
           programName +
           " <input-image> [options]\n"
           "\n"
           "Options:\n"
           "  -o, --output <path>       Output GIF path (default: it_pixels_transform.gif\n"
           "                            next to the input image)\n"
           "  -r, --reference <path>    Reference image (default: bundled it.jpeg asset)\n"
           "  -b, --block-size <n>      Mosaic block edge in pixels, 1-1024 (default: 1)\n"
           "  -s, --steps <n>           Animation batches, 1-10000 (default: 120)\n"
           "      --blend <0..1>        Target colour blend, 1.0 = flat colours (default: 1.0)\n"
           "      --fps <1..50>         GIF playback rate (default: 25)\n"
           "      --frame-sample <n>    Keep every Nth animation frame, >= 1 (default: 1)\n"
           "      --seed <n>            Random seed for reproducible output (default: random)\n"
           "  -h, --help                Show this help\n"
           "\n"
           "Exit codes:\n"
           "  0  success\n"
           "  1  processing error (missing file, bad image, write failure)\n"
           "  2  usage error\n";
}

std::expected<CliOptions, std::string> CliOptions::parse(const std::vector<std::string>& args) {
    CliOptions options;
    bool haveInput = false;

    const auto fail = [](std::string message) {
        return std::expected<CliOptions, std::string>(std::unexpected(std::move(message)));
    };

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        const auto value = [&](const char* name) -> std::expected<std::string, std::string> {
            if (i + 1 >= args.size()) {
                return std::unexpected(std::string(name) + " requires a value");
            }
            return args[++i];
        };

        if (arg == "-h" || arg == "--help") {
            return fail("help");
        } else if (arg == "-o" || arg == "--output") {
            auto v = value("--output");
            if (!v) {
                return fail(v.error());
            }
            options.output = std::filesystem::path(*v);
        } else if (arg == "-r" || arg == "--reference") {
            auto v = value("--reference");
            if (!v) {
                return fail(v.error());
            }
            options.reference = std::filesystem::path(*v);
        } else if (arg == "-b" || arg == "--block-size") {
            auto v = value("--block-size");
            if (!v) {
                return fail(v.error());
            }
            long long n{};
            if (!parseInteger(*v, n) || n < kMinBlock || n > kMaxBlock) {
                return fail("block size must be an integer between 1 and 1024");
            }
            options.settings.blockSize = static_cast<int>(n);
        } else if (arg == "-s" || arg == "--steps") {
            auto v = value("--steps");
            if (!v) {
                return fail(v.error());
            }
            long long n{};
            if (!parseInteger(*v, n) || n < kMinSteps || n > kMaxSteps) {
                return fail("steps must be an integer between 1 and 10000");
            }
            options.settings.animationSteps = static_cast<int>(n);
        } else if (arg == "--blend") {
            auto v = value("--blend");
            if (!v) {
                return fail(v.error());
            }
            double d{};
            if (!parseDouble(*v, d) || d < 0.0 || d > 1.0) {
                return fail("blend must be a number between 0.0 and 1.0");
            }
            options.settings.blendStrength = d;
        } else if (arg == "--fps") {
            auto v = value("--fps");
            if (!v) {
                return fail(v.error());
            }
            long long n{};
            if (!parseInteger(*v, n) || n < kMinFps || n > kMaxFps) {
                return fail("fps must be an integer between 1 and 50");
            }
            options.fps = static_cast<int>(n);
        } else if (arg == "--frame-sample") {
            auto v = value("--frame-sample");
            if (!v) {
                return fail(v.error());
            }
            long long n{};
            if (!parseInteger(*v, n) || n < 1) {
                return fail("frame sample must be an integer >= 1");
            }
            options.frameSample = static_cast<int>(n);
        } else if (arg == "--seed") {
            auto v = value("--seed");
            if (!v) {
                return fail(v.error());
            }
            long long n{};
            if (!parseInteger(*v, n) || n < 0 || n > 0xffffffffLL) {
                return fail("seed must be an integer between 0 and 4294967295");
            }
            options.seed = static_cast<std::uint32_t>(n);
        } else if (!arg.empty() && arg[0] == '-') {
            return fail("unknown option: " + arg);
        } else {
            if (haveInput) {
                return fail("unexpected extra argument: " + arg);
            }
            options.input = std::filesystem::path(arg);
            haveInput = true;
        }
    }

    if (!haveInput) {
        return fail("an input image path is required");
    }

    if (options.output.empty()) {
        // Match the original tool: write next to the input image.
        options.output = options.input.parent_path() / "it_pixels_transform.gif";
    }
    return options;
}

} // namespace itpixel::cli
