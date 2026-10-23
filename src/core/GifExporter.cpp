#include "itpixel/GifExporter.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <vector>

extern "C" {
#include "gif_lib.h"

// Quantize lives in the vendored quantize.c but has no gif_lib.h prototype.
// The declaration must match the upstream giflib signature exactly.
// NOLINTBEGIN(readability-identifier-naming)
int GifQuantizeBuffer(unsigned int Width, unsigned int Height, int* ColorMapSize,
                      const GifByteType* RedInput, const GifByteType* GreenInput,
                      const GifByteType* BlueInput, GifByteType* OutputBuffer,
                      GifColorType* OutputColorMap);
// NOLINTEND(readability-identifier-naming)
}

namespace itpixel {
namespace {

constexpr int kPaletteSize = 256;

// Byte offset of a pixel inside an RGB888 row.
inline std::size_t pixelOffset(int x) {
    return static_cast<std::size_t>(x) * 3;
}

// Disposal method 1 ("leave frame in place"); every frame is complete.
constexpr int kDisposalKeep = 1;

// The NETSCAPE2.0 application extension asks viewers to loop the animation.
constexpr char kNetscapeId[] = "NETSCAPE2.0"; // NOLINT(modernize-avoid-c-arrays)

} // namespace

struct GifExporter::Impl {
    GifFileType* gif{};
    QSize size{};
    int delayCs{};                // frame delay in centiseconds
    std::vector<GifByteType> red; // planar buffers, reused across frames
    std::vector<GifByteType> green;
    std::vector<GifByteType> blue;
    std::vector<GifByteType> indices;
    std::vector<GifColorType> palette;
    bool finished{};
    bool failed{};
};

GifExporter::GifExporter(GifExporter&& other) noexcept
    : impl_(std::exchange(other.impl_, nullptr)) {}

GifExporter& GifExporter::operator=(GifExporter&& other) noexcept {
    if (this != &other) {
        finish();
        impl_ = std::exchange(other.impl_, nullptr);
    }
    return *this;
}

GifExporter::~GifExporter() {
    if (impl_ != nullptr) {
        finish();
        delete impl_;
    }
}

std::optional<GifExporter> GifExporter::create(const std::filesystem::path& path, QSize size,
                                               Options options, std::string* error) {
    if (size.isEmpty()) {
        if (error != nullptr) {
            *error = "GIF frame size must be positive";
        }
        return std::nullopt;
    }
    options.fps = std::clamp(options.fps, 1, 50);
    options.loopCount = std::clamp(options.loopCount, 0, 65'535);

    // Deletion instead of truncation so a failure below does not leave a
    // half-written file masquerading as a valid animation.
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::filesystem::remove(path, ec);
    }

    int gifError = GIF_OK;
    GifFileType* gif = EGifOpenFileName(path.string().c_str(), false, &gifError);
    if (gif == nullptr) {
        if (error != nullptr) {
            *error = "cannot open output file: " +
                     (gifError != 0 ? std::string(GifErrorString(gifError)) : std::strerror(errno));
        }
        return std::nullopt;
    }

    // GIF89a is required for the graphics control and loop extensions.
    EGifSetGifVersion(gif, true);

    const bool headerOk =
        EGifPutScreenDesc(gif, size.width(), size.height(), 8, 0, nullptr) != GIF_ERROR &&
        EGifPutExtensionLeader(gif, APPLICATION_EXT_FUNC_CODE) != GIF_ERROR &&
        EGifPutExtensionBlock(gif, sizeof(kNetscapeId) - 1,
                              const_cast<GifByteType*>(
                                  reinterpret_cast<const GifByteType*>(kNetscapeId))) != GIF_ERROR;
    bool loopOk = false;
    if (headerOk) {
        // Loop-count sub-block: 0x01, little-endian count (0 = loop forever).
        const std::array<GifByteType, 3> loop = {0x01,
                                                 static_cast<GifByteType>(options.loopCount & 0xff),
                                                 static_cast<GifByteType>(options.loopCount >> 8)};
        loopOk = EGifPutExtensionBlock(gif, 3, loop.data()) != GIF_ERROR &&
                 EGifPutExtensionTrailer(gif) != GIF_ERROR;
    }
    if (!headerOk || !loopOk) {
        int closeError = GIF_OK;
        (void)EGifCloseFile(gif, &closeError);
        if (error != nullptr) {
            *error = "failed to write GIF header";
        }
        return std::nullopt;
    }

    GifExporter exporter;
    exporter.impl_ = new Impl;
    exporter.impl_->gif = gif;
    exporter.impl_->size = size;
    // GIF delays are centiseconds; 25 fps -> 4 cs.
    exporter.impl_->delayCs = std::clamp((100 + options.fps / 2) / options.fps, 1, 100);
    const std::size_t pixels =
        static_cast<std::size_t>(size.width()) * static_cast<std::size_t>(size.height());
    exporter.impl_->red.resize(pixels);
    exporter.impl_->green.resize(pixels);
    exporter.impl_->blue.resize(pixels);
    exporter.impl_->indices.resize(pixels);
    exporter.impl_->palette.resize(kPaletteSize);
    return exporter;
}

bool GifExporter::addFrame(const QImage& frame) {
    if (impl_ == nullptr || impl_->finished) {
        return false;
    }
    if (frame.isNull() || frame.size() != impl_->size) {
        return false;
    }
    const QImage rgb = frame.format() == QImage::Format_RGB888
                           ? frame
                           : frame.convertToFormat(QImage::Format_RGB888);
    const int width = rgb.width();
    const int height = rgb.height();

    const auto w = static_cast<std::size_t>(width);
    for (int y = 0; y < height; ++y) {
        const uchar* src = rgb.constScanLine(y);
        GifByteType* r = impl_->red.data() + static_cast<std::size_t>(y) * w;
        GifByteType* g = impl_->green.data() + static_cast<std::size_t>(y) * w;
        GifByteType* b = impl_->blue.data() + static_cast<std::size_t>(y) * w;
        for (int x = 0; x < width; ++x) {
            const std::size_t off = pixelOffset(x);
            r[x] = src[off + 0];
            g[x] = src[off + 1];
            b[x] = src[off + 2];
        }
    }

    int colorMapSize = kPaletteSize;
    if (GifQuantizeBuffer(static_cast<unsigned>(width), static_cast<unsigned>(height),
                          &colorMapSize, impl_->red.data(), impl_->green.data(), impl_->blue.data(),
                          impl_->indices.data(), impl_->palette.data()) == GIF_ERROR) {
        impl_->failed = true;
        return false;
    }

    // Graphics control extension: disposal + delay, no transparency.
    const std::array<GifByteType, 4> gce = {
        static_cast<GifByteType>(kDisposalKeep << 2),
        static_cast<GifByteType>(impl_->delayCs & 0xff),
        static_cast<GifByteType>(impl_->delayCs >> 8),
        0x00,
    };
    if (EGifPutExtension(impl_->gif, GRAPHICS_EXT_FUNC_CODE, 4, gce.data()) == GIF_ERROR) {
        impl_->failed = true;
        return false;
    }

    // giflib only accepts color maps whose size is a power of two, and GIF
    // needs at least a 2-entry map; entries between the quantized size and
    // the rounded size stay unused (black).
    int roundedSize = 2;
    while (roundedSize < colorMapSize) {
        roundedSize <<= 1;
    }
    auto* colorMap = GifMakeMapObject(roundedSize, impl_->palette.data());
    if (colorMap == nullptr) {
        impl_->failed = true;
        return false;
    }
    const bool descOk =
        EGifPutImageDesc(impl_->gif, 0, 0, width, height, false, colorMap) != GIF_ERROR;
    bool linesOk = true;
    if (descOk) {
        const auto rowWidth = static_cast<std::size_t>(width);
        for (int y = 0; y < height; ++y) {
            if (EGifPutLine(impl_->gif,
                            impl_->indices.data() + static_cast<std::size_t>(y) * rowWidth,
                            width) == GIF_ERROR) {
                linesOk = false;
                break;
            }
        }
    }
    GifFreeMapObject(colorMap);
    if (!descOk || !linesOk) {
        impl_->failed = true;
        return false;
    }
    return true;
}

bool GifExporter::finish() {
    if (impl_ == nullptr || impl_->finished) {
        return !impl_ || !impl_->failed;
    }
    impl_->finished = true;
    int closeError = GIF_OK;
    const bool closed = EGifCloseFile(impl_->gif, &closeError) != GIF_ERROR;
    impl_->gif = nullptr;
    return closed && !impl_->failed;
}

} // namespace itpixel
