#include "doctest.h"

#include <filesystem>
#include <string>
#include <vector>

#include "CliOptions.hpp"

namespace {

std::vector<std::string> asArgs(std::initializer_list<std::string> items) {
    return items;
}

itpixel::cli::CliOptions parseOr(std::initializer_list<std::string> items) {
    auto parsed = itpixel::cli::CliOptions::parse(asArgs(items));
    REQUIRE(parsed.has_value());
    return std::move(*parsed);
}

} // namespace

TEST_CASE("defaults match the original tool") {
    const auto options = parseOr({"photo.png"});
    CHECK(options.input == std::filesystem::path("photo.png"));
    // Default output goes next to the input image.
    CHECK(options.output == std::filesystem::path("it_pixels_transform.gif"));
    CHECK(options.settings.blockSize == 1);
    CHECK(options.settings.animationSteps == 120);
    CHECK(options.settings.blendStrength == 1.0);
    CHECK(options.fps == 25);
    CHECK(options.frameSample == 1);
    CHECK_FALSE(options.seed.has_value());
}

TEST_CASE("all original options are accepted") {
    const auto options =
        parseOr({"in/my photo.png", "--output", "out dir/my clip.gif", "--reference", "ref img.jpg",
                 "--block-size", "4", "--steps", "200", "--blend", "0.85", "--fps", "12",
                 "--frame-sample", "3", "--seed", "77"});
    CHECK(options.input == std::filesystem::path("in/my photo.png"));
    CHECK(options.output == std::filesystem::path("out dir/my clip.gif"));
    CHECK(options.reference == std::filesystem::path("ref img.jpg"));
    CHECK(options.settings.blockSize == 4);
    CHECK(options.settings.animationSteps == 200);
    CHECK(options.settings.blendStrength == 0.85);
    CHECK(options.fps == 12);
    CHECK(options.frameSample == 3);
    CHECK(options.seed == 77);
}

TEST_CASE("short option forms are accepted") {
    const auto options = parseOr({"a.png", "-o", "x.gif", "-r", "r.jpg", "-b", "3", "-s", "9"});
    CHECK(options.output == std::filesystem::path("x.gif"));
    CHECK(options.reference == std::filesystem::path("r.jpg"));
    CHECK(options.settings.blockSize == 3);
    CHECK(options.settings.animationSteps == 9);
}

TEST_CASE("invalid arguments produce usage errors") {
    const auto expectError = [](std::initializer_list<std::string> items) {
        auto parsed = itpixel::cli::CliOptions::parse(asArgs(items));
        REQUIRE_FALSE(parsed.has_value());
        CHECK_FALSE(parsed.error().empty());
        return parsed.error();
    };

    CHECK(expectError({}) == "an input image path is required");
    CHECK(expectError({"--bogus", "x.png"}) == "unknown option: --bogus");
    CHECK(expectError({"a.png", "b.png"}).find("extra argument") != std::string::npos);
    CHECK(expectError({"a.png", "--steps", "0"}).find("steps") != std::string::npos);
    CHECK(expectError({"a.png", "--steps", "100001"}).find("steps") != std::string::npos);
    CHECK(expectError({"a.png", "--block-size", "2000"}).find("block size") != std::string::npos);
    CHECK(expectError({"a.png", "--blend", "1.5"}).find("blend") != std::string::npos);
    CHECK(expectError({"a.png", "--blend", "abc"}).find("blend") != std::string::npos);
    CHECK(expectError({"a.png", "--fps", "0"}).find("fps") != std::string::npos);
    CHECK(expectError({"a.png", "--frame-sample", "-2"}).find("frame sample") != std::string::npos);
    CHECK(expectError({"a.png", "--seed", "8589934592"}).find("seed") != std::string::npos);
    CHECK(expectError({"a.png", "--seed", "abc"}).find("seed") != std::string::npos);
    CHECK(expectError({"a.png", "--output"}) == "--output requires a value");
}

TEST_CASE("output default is relative to the input directory") {
    const auto options = parseOr({"/data/pics/my input.png"});
    CHECK(options.output == std::filesystem::path("/data/pics/it_pixels_transform.gif"));
}

TEST_CASE("help is requested through a sentinel error") {
    const auto parsed = itpixel::cli::CliOptions::parse({"--help"});
    REQUIRE_FALSE(parsed.has_value());
    CHECK(parsed.error() == "help");
}

TEST_CASE("help text documents every option") {
    const std::string help = itpixel::cli::CliOptions::helpText("it-pixel-cli");
    const auto contains = [&help](const char* needle) {
        return help.find(needle) != std::string::npos;
    };
    CHECK(contains("--output"));
    CHECK(contains("--reference"));
    CHECK(contains("--block-size"));
    CHECK(contains("--steps"));
    CHECK(contains("--blend"));
    CHECK(contains("--fps"));
    CHECK(contains("--frame-sample"));
    CHECK(contains("--seed"));
}
