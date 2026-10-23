#include "doctest.h"

#include <filesystem>
#include <cstring>
#include <string>
#include <vector>

#include <QImage>
#include <QSize>
#include <QTemporaryDir>

#include "itpixel/GifExporter.hpp"
#include "itpixel/MosaicEngine.hpp"

extern "C" {
#include "gif_lib.h"
}

namespace {

// giflib decoder used as a real-world verification of what we wrote.
struct DecodedGif {
    int frames{};
    int width{};
    int height{};
    bool loopExtension{};
};

std::string decode(const std::filesystem::path& path, DecodedGif* out = nullptr)
{
    int error = GIF_OK;
    GifFileType* gif = DGifOpenFileName(path.string().c_str(), &error);
    if (gif == nullptr) {
        return "cannot open GIF (error " + std::to_string(error) + ")";
    }
    DecodedGif decoded;
    decoded.width = gif->SWidth;
    decoded.height = gif->SHeight;
    if (DGifSlurp(gif) == GIF_ERROR) {
        DGifCloseFile(gif, &error);
        return "cannot decode GIF data";
    }
    decoded.frames = gif->ImageCount;
    // DGifSlurp attaches extension blocks to the image that follows them.
    // The loop extension is an application block (0xff) whose first
    // sub-block identifies "NETSCAPE2.0"; the decoder reports the following
    // sub-block with function code 0.
    for (int i = 0; i < gif->ImageCount; ++i) {
        const SavedImage& image = gif->SavedImages[i];
        for (int b = 0; b < image.ExtensionBlockCount; ++b) {
            const ExtensionBlock& block = image.ExtensionBlocks[b];
            if (block.Function == APPLICATION_EXT_FUNC_CODE && block.ByteCount >= 11 &&
                std::memcmp(block.Bytes, "NETSCAPE2.0", 11) == 0) {
                decoded.loopExtension = true;
            }
        }
    }
    DGifCloseFile(gif, &error);

    if (decoded.frames <= 0) {
        return "no frames decoded";
    }
    if (out != nullptr) {
        *out = decoded;
    }
    return {};
}

QImage testFrame(int width, int height, int shade)
{
    QImage frame(width, height, QImage::Format_RGB888);
    for (int y = 0; y < height; ++y) {
        uchar* row = frame.scanLine(y);
        for (int x = 0; x < width; ++x) {
            row[static_cast<std::size_t>(3 * x)] = static_cast<uchar>((x + shade * 8) % 256);
            row[static_cast<std::size_t>(3 * x) + 1] = static_cast<uchar>((y + shade * 16) % 256);
            row[static_cast<std::size_t>(3 * x) + 2] = static_cast<uchar>((shade * 32) % 256);
        }
    }
    return frame;
}

}  // namespace

TEST_CASE("GIF exporter writes a decodable animation")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = std::filesystem::path(dir.path().toStdString()) / "anim.gif";

    std::string error;
    auto exporter = itpixel::GifExporter::create(
        path, QSize(32, 24), itpixel::GifExporter::Options{25, 0}, &error);
    REQUIRE(exporter.has_value());

    for (int i = 0; i < 5; ++i) {
        REQUIRE(exporter->addFrame(testFrame(32, 24, i)));
    }
    REQUIRE(exporter->finish());

    // GIF89a signature.
    QFile file(QString::fromStdString(path.string()));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QByteArray header = file.read(6);
    file.close();
    CHECK(header == QByteArrayLiteral("GIF89a"));

    DecodedGif decoded;
    const std::string decodeError = decode(path, &decoded);
    CHECK(decodeError.empty());
    CHECK(decoded.frames == 5);
    CHECK(decoded.width == 32);
    CHECK(decoded.height == 24);
    CHECK(decoded.loopExtension);
}

TEST_CASE("GIF exporter fails for unwritable paths")
{
    std::string error;
    const auto exporter = itpixel::GifExporter::create(
        "/nonexistent-directory-xyz/out.gif", QSize(8, 8),
        itpixel::GifExporter::Options{25, 0}, &error);
    CHECK_FALSE(exporter.has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("GIF exporter rejects mismatched frame sizes")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = std::filesystem::path(dir.path().toStdString()) / "bad.gif";
    std::string error;
    auto exporter =
        itpixel::GifExporter::create(path, QSize(8, 8), {25, 0}, &error);
    REQUIRE(exporter.has_value());
    CHECK_FALSE(exporter->addFrame(testFrame(16, 16, 0)));
    CHECK(exporter->finish());
}

TEST_CASE("full pipeline produces a decodable animated GIF")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const auto path = std::filesystem::path(dir.path().toStdString()) / "pipeline.gif";

    QImage input(16, 12, QImage::Format_RGB888);
    for (int y = 0; y < 12; ++y) {
        uchar* row = input.scanLine(y);
        for (int x = 0; x < 16; ++x) {
            row[static_cast<std::size_t>(3 * x)] = static_cast<uchar>(x * 16);
            row[static_cast<std::size_t>(3 * x) + 1] = static_cast<uchar>(y * 20);
            row[static_cast<std::size_t>(3 * x) + 2] = 0x80;
        }
    }
    QImage reference(6, 6, QImage::Format_RGB888);
    reference.fill(Qt::magenta);

    itpixel::MosaicSettings settings;
    settings.animationSteps = 4;
    settings.blendStrength = 0.8;
    settings.seed = 99;
    itpixel::MosaicEngine engine(input.copy(), reference.copy(), settings);

    std::string error;
    auto exporter = itpixel::GifExporter::create(
        path, engine.canvasSize(), itpixel::GifExporter::Options{25, 0}, &error);
    REQUIRE(exporter.has_value());

    const itpixel::FrameSink sink = [&exporter](const QImage& frame, bool isFinal) {
        // every frame, plus the final one
        CHECK(exporter->addFrame(frame));
        (void)isFinal;
    };
    const itpixel::EngineCallbacks callbacks{[](std::string_view) {}, [](int) {}, nullptr};
    REQUIRE(engine.run(callbacks, sink));
    REQUIRE(exporter->finish());

    // scan frames (12 rows, interval 1) + 4 + 10 animation + 1 final
    QFile check(QString::fromStdString(path.string()));
    REQUIRE(check.exists());
    DecodedGif decoded;
    const std::string decodeError = decode(path, &decoded);
    CHECK(decodeError.empty());
    CHECK(decoded.frames == 12 + 4 + 10 + 1);
}

TEST_CASE("final PNG and JPEG saving works through Qt image writers")
{
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    QImage image = testFrame(16, 16, 3);

    const QString pngPath = QDir(dir.path()).filePath("out.png");
    CHECK(image.save(pngPath, "PNG"));
    CHECK(QFile(pngPath).size() > 0);

    const QString jpgPath = QDir(dir.path()).filePath("out.jpg");
    CHECK(image.save(jpgPath, "JPEG", 95));
    CHECK(QFile(jpgPath).size() > 0);
}
