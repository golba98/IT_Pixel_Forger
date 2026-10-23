#include "doctest.h"

#include <algorithm>
#include <cstddef>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QIODevice>
#include <QSize>
#include <QString>
#include <vector>

#include "itpixel/MosaicEngine.hpp"
#include "itpixel/MosaicSettings.hpp"

namespace {

// Small solid-color test image.
QImage solidImage(int width, int height, QRgb color)
{
    QImage image(width, height, QImage::Format_RGB888);
    image.fill(color);
    return image;
}

QImage gradientImage(int width, int height)
{
    QImage image(width, height, QImage::Format_RGB888);
    for (int y = 0; y < height; ++y) {
        uchar* row = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            row[static_cast<std::size_t>(3 * x)] = static_cast<uchar>((x * 255) / std::max(1, width - 1));
            row[static_cast<std::size_t>(3 * x) + 1] = static_cast<uchar>((y * 255) / std::max(1, height - 1));
            row[static_cast<std::size_t>(3 * x) + 2] = 128;
        }
    }
    return image;
}

struct RunResult {
    bool completed{};
    QImage finalFrame;
    int frameCount{};
    std::vector<int> progressLog;
};

RunResult runEngine(QImage input, QImage reference, itpixel::MosaicSettings settings,
                   std::size_t* cancelledAfter = nullptr)
{
    itpixel::MosaicEngine engine(std::move(input), std::move(reference), settings);
    RunResult result;
    std::size_t calls = 0;
    const itpixel::FrameSink sink = [&](const QImage& frame, bool isFinal) {
        ++result.frameCount;
        if (isFinal) {
            result.finalFrame = frame.copy();
        }
    };
    const itpixel::EngineCallbacks callbacks{
        [](std::string_view) {},
        [&](int percent) { result.progressLog.push_back(percent); },
        [&]() {
            if (cancelledAfter != nullptr && ++calls >= *cancelledAfter) {
                return true;
            }
            return false;
        }};
    result.completed = engine.run(callbacks, sink);
    return result;
}

}  // namespace

TEST_CASE("blend 1.0 paints exact reference block means")
{
    // Reference: 2x2 blocks of solid color in a 4x4 image. With blend 1.0
    // every block becomes the plain target colour, so the final frame is
    // fully deterministic regardless of tile selection.
    QImage reference(4, 4, QImage::Format_RGB888);
    const QRgb quadrants[4] = {qRgb(200, 20, 20), qRgb(20, 200, 20),
                               qRgb(20, 20, 200), qRgb(180, 180, 40)};
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            reference.setPixel(x, y, quadrants[(y / 2) * 2 + x / 2]);
        }
    }
    itpixel::MosaicSettings settings;
    settings.blockSize = 2;
    settings.blendStrength = 1.0;
    settings.animationSteps = 3;

    const RunResult result =
        runEngine(gradientImage(4, 4), reference.copy(), settings);

    REQUIRE(result.completed);
    REQUIRE(result.finalFrame.size() == QSize(4, 4));
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const QRgb expected = quadrants[(y / 2) * 2 + x / 2];
            const QRgb actual = result.finalFrame.pixel(x, y);
            CAPTURE(x);
            CAPTURE(y);
            CAPTURE(expected);
            CAPTURE(actual);
            CHECK(qRed(actual) == qRed(expected));
            CHECK(qGreen(actual) == qGreen(expected));
            CHECK(qBlue(actual) == qBlue(expected));
        }
    }
}

TEST_CASE("deterministic output for identical seeds")
{
    itpixel::MosaicSettings settings;
    settings.blockSize = 1;
    settings.animationSteps = 8;
    settings.blendStrength = 0.6;
    settings.seed = 42;

    const QImage input = gradientImage(24, 16);
    const QImage reference = gradientImage(8, 8);
    const RunResult a = runEngine(input.copy(), reference.copy(), settings);
    const RunResult b = runEngine(input.copy(), reference.copy(), settings);
    REQUIRE(a.completed);
    REQUIRE(b.completed);
    CHECK(a.finalFrame == b.finalFrame);

    // A different seed must be able to select different tiles (blend < 1).
    settings.seed = 1337;
    const RunResult c = runEngine(input.copy(), reference.copy(), settings);
    REQUIRE(c.completed);
    CHECK_FALSE(a.finalFrame == c.finalFrame);
}

TEST_CASE("progress is monotonic and completes at 100")
{
    itpixel::MosaicSettings settings;
    settings.animationSteps = 5;
    const RunResult result =
        runEngine(gradientImage(20, 10), gradientImage(6, 6), settings);
    REQUIRE(result.completed);
    REQUIRE_FALSE(result.progressLog.empty());
    CHECK(result.progressLog.front() == 0);
    CHECK(result.progressLog.back() == 100);
    CHECK(std::is_sorted(result.progressLog.begin(), result.progressLog.end()));
}

TEST_CASE("produces the documented number of frames")
{
    // scan frames + (steps + 10) animation frames + 1 final frame
    itpixel::MosaicSettings settings;
    settings.blockSize = 1;
    settings.animationSteps = 5;
    const RunResult result =
        runEngine(gradientImage(32, 24), gradientImage(6, 6), settings);
    REQUIRE(result.completed);
    // height 24 -> scan interval max(1, 24/15) = 1 -> one preview per row.
    CHECK(result.frameCount == 24 + (5 + 10) + 1);
}

TEST_CASE("cancellation stops the run")
{
    itpixel::MosaicSettings settings;
    settings.animationSteps = 40;
    std::size_t cancelAfter = 3;
    const RunResult result =
        runEngine(gradientImage(32, 24), gradientImage(6, 6), settings, &cancelAfter);
    CHECK_FALSE(result.completed);
}

TEST_CASE("1x1 image produces a single blended block")
{
    itpixel::MosaicSettings settings;
    settings.blockSize = 1;
    settings.animationSteps = 1;
    settings.blendStrength = 1.0;
    const RunResult result =
        runEngine(solidImage(1, 1, qRgb(10, 20, 30)), solidImage(4, 4, qRgb(200, 100, 50)),
                  settings);
    REQUIRE(result.completed);
    const QRgb px = result.finalFrame.pixel(0, 0);
    CHECK(qRed(px) == 200);
    CHECK(qGreen(px) == 100);
    CHECK(qBlue(px) == 50);
}

TEST_CASE("odd dimensions and oversized blocks are handled")
{
    SUBCASE("3x5 image with block size 2")
    {
        itpixel::MosaicSettings settings;
        settings.blockSize = 2;
        settings.animationSteps = 2;
        settings.blendStrength = 1.0;
        const RunResult result =
            runEngine(gradientImage(3, 5), solidImage(9, 9, qRgb(90, 90, 200)), settings);
        REQUIRE(result.completed);
        CHECK(result.finalFrame.size() == QSize(3, 5));
    }
    SUBCASE("block size larger than the image collapses to one block")
    {
        itpixel::MosaicSettings settings;
        settings.blockSize = 64;
        settings.animationSteps = 1;
        settings.blendStrength = 1.0;
        const RunResult result =
            runEngine(gradientImage(4, 4), solidImage(8, 8, qRgb(20, 200, 20)), settings);
        REQUIRE(result.completed);
        // One block, filled entirely with the single reference mean color.
        for (int y = 0; y < 4; ++y) {
            for (int x = 0; x < 4; ++x) {
                const QRgb px = result.finalFrame.pixel(x, y);
                CHECK(qRed(px) == 20);
                CHECK(qGreen(px) == 200);
                CHECK(qBlue(px) == 20);
            }
        }
    }
}

TEST_CASE("large-image block policy caps the block count")
{
    using itpixel::MosaicEngine;
    // Below the cap nothing changes.
    CHECK(MosaicEngine::effectiveBlockSizeFor(QSize(1920, 1080), 1) == 1);
    // 8K image (33M blocks) raises the block size.
    CHECK(MosaicEngine::effectiveBlockSizeFor(QSize(7680, 4320), 1) == 2);
    CHECK(MosaicEngine::effectiveBlockSizeFor(QSize(16384, 16384), 1) == 6);
    // Explicit block sizes are respected.
    CHECK(MosaicEngine::effectiveBlockSizeFor(QSize(7680, 4320), 4) == 4);
    CHECK(MosaicEngine::effectiveBlockSizeFor(QSize(16, 16), 1) == 1);
}

TEST_CASE("settings are validated and clamped")
{
    using itpixel::MosaicSettings;
    MosaicSettings s;
    s.blockSize = 0;
    s.animationSteps = -5;
    s.blendStrength = 2.0;
    const MosaicSettings v = MosaicSettings::validated(s);
    CHECK(v.blockSize == 1);
    CHECK(v.animationSteps == 1);
    CHECK(v.blendStrength == 1.0);
}

TEST_CASE("loadImage reports missing and invalid files")
{
    QString error;
    CHECK_FALSE(itpixel::MosaicEngine::loadImage(
        QStringLiteral("/nonexistent/image.png"), &error).has_value());
    CHECK_FALSE(error.isEmpty());

    // A text file is not an image.
    const QString textPath = QDir::temp().filePath(QStringLiteral("itpixel-not-an-image.png"));
    QFile file(textPath);
    if (!file.open(QIODevice::WriteOnly)) {
        FAIL("cannot create fixture file");
    }
    file.write("this is not an image");
    file.close();
    error.clear();
    CHECK_FALSE(itpixel::MosaicEngine::loadImage(textPath, &error).has_value());
    CHECK_FALSE(error.isEmpty());
    QFile::remove(textPath);
}

TEST_CASE("loadImage reads the bundled JPEG reference asset")
{
    // Verifies JPEG decoding support is present in this Qt build.
    const QString asset = QStringLiteral(ITPIXEL_REFERENCE_ASSET);
    if (!QFile::exists(asset)) {
        FAIL("reference asset missing: ", asset.toStdString());
    }
    QString error;
    const auto image = itpixel::MosaicEngine::loadImage(asset, &error);
    REQUIRE(image.has_value());
    CHECK(image->width() == 1920);
    CHECK(image->height() == 1080);
}

TEST_CASE("engine rejects null images")
{
    CHECK_THROWS_AS(
        itpixel::MosaicEngine(QImage(), QImage(), itpixel::MosaicSettings{}),
        std::invalid_argument);
}

TEST_CASE("repeated runs with the same inputs stay deterministic")
{
    itpixel::MosaicSettings settings;
    settings.animationSteps = 12;
    settings.blendStrength = 0.5;
    settings.seed = 7;
    const QImage input = gradientImage(48, 32);
    const QImage reference = gradientImage(12, 12);
    const RunResult first = runEngine(input.copy(), reference.copy(), settings);
    // Run twice more; engine state must not leak between runs.
    for (int i = 0; i < 2; ++i) {
        const RunResult again = runEngine(input.copy(), reference.copy(), settings);
        CHECK(again.finalFrame == first.finalFrame);
    }
}
