#include "itpixel/MosaicEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>

#include <QImageReader>

namespace itpixel {
namespace {

// Luminance weights used by the original engine (ITU-R BT.601).
constexpr double kLumR = 0.299;
constexpr double kLumG = 0.587;
constexpr double kLumB = 0.114;

// Visual constants carried over from the original implementation.
constexpr double kDimFactor = 0.3;     // scan-preview background brightness
constexpr double kShimmerFactor = 1.4; // "sparkle" applied to the active batch
constexpr int kScanBands = 15;         // scan-preview frames per run
constexpr int kPaddingSteps = 10;      // tail batches after all blocks are placed
constexpr std::array<std::uint8_t, 3> kGreen = {0x00, 0xff, 0x00};

// Progress window occupied by each phase, in percent.
constexpr int kProgressScanEnd = 40;
constexpr int kProgressGridEnd = 65;
constexpr int kProgressMax = 100;

double luminance(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return kLumR * r + kLumG * g + kLumB * b;
}

// Hue component of HSV in [0, 1), matching Python's colorsys.rgb_to_hsv.
double hue(double r, double g, double b) {
    const double maxc = std::max({r, g, b});
    const double minc = std::min({r, g, b});
    if (maxc <= minc) {
        return 0.0;
    }
    const double rc = (maxc - r) / (maxc - minc);
    const double gc = (maxc - g) / (maxc - minc);
    const double bc = (maxc - b) / (maxc - minc);
    double h{};
    if (r == maxc) {
        h = bc - gc;
    } else if (g == maxc) {
        h = 2.0 + rc - bc;
    } else {
        h = 4.0 + gc - rc;
    }
    h /= 6.0;
    return h < 0.0 ? h + 1.0 : h;
}

// Brightness scaling with the same rounding as Pillow's blend (factor * x + 0.5).
inline std::uint8_t brighten(std::uint8_t v, double factor) {
    // Values are non-negative, so lround matches the original engine's
    // truncation of (factor * v + 0.5) exactly.
    const auto scaled = static_cast<int>(std::lround(v * factor));
    return static_cast<std::uint8_t>(std::clamp(scaled, 0, 255));
}

struct Rgb {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
};

// Byte offset of a pixel inside an RGB888 row.
inline std::size_t pixelOffset(int x) {
    return static_cast<std::size_t>(x) * 3;
}

Rgb pixelAt(const QImage& image, int x, int y) {
    // RGB888 guarantees 3 bytes per pixel; scanLine handles stride.
    const uchar* row = image.scanLine(y) + pixelOffset(x);
    return {.r = row[0], .g = row[1], .b = row[2]};
}

// True arithmetic mean of a block. The original engine used
// Pillow's resize((1,1)) which is not an exact average for blocks larger
// than one pixel; the exact mean is the corrected, documented behaviour.
Rgb averageColor(const QImage& image, int x, int y, int w, int h) {
    if (w == 1 && h == 1) {
        return pixelAt(image, x, y);
    }
    std::uint64_t sr{};
    std::uint64_t sg{};
    std::uint64_t sb{};
    for (int py = y; py < y + h; ++py) {
        const uchar* row = image.scanLine(py);
        for (int pxx = x; pxx < x + w; ++pxx) {
            const uchar* p = row + pixelOffset(pxx);
            sr += p[0];
            sg += p[1];
            sb += p[2];
        }
    }
    const auto n = static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h);
    return {.r = static_cast<std::uint8_t>(sr / n),
            .g = static_cast<std::uint8_t>(sg / n),
            .b = static_cast<std::uint8_t>(sb / n)};
}

} // namespace

MosaicSettings MosaicSettings::validated(MosaicSettings s) {
    s.blockSize = std::clamp(s.blockSize, 1, 1024);
    s.animationSteps = std::clamp(s.animationSteps, 1, 10'000);
    s.blendStrength = std::clamp(s.blendStrength, 0.0, 1.0);
    return s;
}

LoadResult MosaicEngine::loadImage(const QString& path, QString* error) {
    QImageReader reader(path);
    reader.setAutoTransform(false);
    QImage image = reader.read();
    if (image.isNull()) {
        if (error != nullptr) {
            *error = QStringLiteral("%1: %2").arg(path, reader.errorString());
        }
        return std::nullopt;
    }
    return image.convertToFormat(QImage::Format_RGB888);
}

QImage MosaicEngine::scaled(const QImage& image, QSize size) {
    return image
        .scaled(size.width(), size.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .convertToFormat(QImage::Format_RGB888);
}

int MosaicEngine::effectiveBlockSizeFor(QSize size, int requestedBlock) {
    if (requestedBlock != 1) {
        return requestedBlock;
    }
    const std::size_t pixels =
        static_cast<std::size_t>(size.width()) * static_cast<std::size_t>(size.height());
    if (pixels <= MosaicSettings::kMaxWorkingBlocks) {
        return requestedBlock;
    }
    const double factor =
        static_cast<double>(pixels) / static_cast<double>(MosaicSettings::kMaxWorkingBlocks);
    return std::max(1, static_cast<int>(std::lround(std::sqrt(factor))));
}

MosaicEngine::MosaicEngine(QImage input, QImage reference, MosaicSettings settings)
    : input_(std::move(input)), reference_(std::move(reference)),
      settings_(MosaicSettings::validated(settings)) {
    if (input_.isNull() || reference_.isNull()) {
        throw std::invalid_argument("MosaicEngine requires non-null images");
    }
    input_ = input_.convertToFormat(QImage::Format_RGB888);
    reference_ = reference_.convertToFormat(QImage::Format_RGB888);

    settings_.blockSize = effectiveBlockSizeFor(input_.size(), settings_.blockSize);

    reference_ = MosaicEngine::scaled(reference_, input_.size());
    canvas_ = input_.copy();
    dimmedInput_ = input_.copy();
    uchar* bytes = dimmedInput_.bits();
    const qsizetype size = dimmedInput_.sizeInBytes();
    for (qsizetype i = 0; i < size; ++i) {
        bytes[i] = static_cast<uchar>(std::lround(bytes[i] * kDimFactor));
    }
}

bool MosaicEngine::cancelRequested(const EngineCallbacks& callbacks) const {
    return callbacks.cancelled && callbacks.cancelled();
}

void MosaicEngine::reportProgress(const EngineCallbacks& callbacks, int progress) const {
    if (callbacks.progress) {
        callbacks.progress(std::clamp(progress, 0, kProgressMax));
    }
}

void MosaicEngine::scanTiles(const EngineCallbacks& callbacks, const FrameSink& sink) {
    if (callbacks.status) {
        callbacks.status("Phase 1/3: Scanning input pixels");
    }
    const int width = input_.width();
    const int height = input_.height();
    const int block = settings_.blockSize;
    const int cols = (width + block - 1) / block;
    const int rows = (height + block - 1) / block;

    tiles_.clear();
    tiles_.reserve(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));
    tileLuminances_.clear();
    tileLuminances_.reserve(static_cast<std::size_t>(cols) * static_cast<std::size_t>(rows));

    const int scanInterval = std::max(block, height / kScanBands);

    for (int by = 0; by < height; by += block) {
        if (cancelRequested(callbacks)) {
            return;
        }
        if (by % scanInterval < block) {
            emitScanPreview(sink, by);
        }
        const int tileH = std::min(block, height - by);
        for (int bx = 0; bx < width; bx += block) {
            const int tileW = std::min(block, width - bx);
            const Rgb avg = averageColor(input_, bx, by, tileW, tileH);
            tiles_.push_back(Tile{.r = avg.r,
                                  .g = avg.g,
                                  .b = avg.b,
                                  .luminance = luminance(avg.r, avg.g, avg.b),
                                  .x = bx,
                                  .y = by,
                                  .w = tileW,
                                  .h = tileH});
        }
        reportProgress(callbacks,
                       static_cast<int>(kProgressScanEnd * (static_cast<double>(by + block) /
                                                            static_cast<double>(height))));
    }

    // Python's sort is stable; keeping stability preserves the original
    // selection order for tiles of equal luminance.
    std::stable_sort(tiles_.begin(), tiles_.end(),
                     [](const Tile& a, const Tile& b) { return a.luminance < b.luminance; });
    tileLuminances_.resize(tiles_.size());
    std::transform(tiles_.begin(), tiles_.end(), tileLuminances_.begin(),
                   [](const Tile& t) { return t.luminance; });
}

void MosaicEngine::buildGrid(const EngineCallbacks& callbacks) {
    if (callbacks.status) {
        callbacks.status("Phase 2/3: Processing target geometry");
    }
    const int width = input_.width();
    const int height = input_.height();
    const int block = settings_.blockSize;

    grid_.clear();
    grid_.reserve(static_cast<std::size_t>((width + block - 1) / block) *
                  static_cast<std::size_t>((height + block - 1) / block));

    for (int by = 0; by < height; by += block) {
        if (cancelRequested(callbacks)) {
            return;
        }
        const int blockH = std::min(block, height - by);
        for (int bx = 0; bx < width; bx += block) {
            const int blockW = std::min(block, width - bx);
            const Rgb target = averageColor(reference_, bx, by, blockW, blockH);
            grid_.push_back(
                GridBlock{.x = bx,
                          .y = by,
                          .w = blockW,
                          .h = blockH,
                          .tr = target.r,
                          .tg = target.g,
                          .tb = target.b,
                          .targetLuminance = luminance(target.r, target.g, target.b),
                          .hue = hue(target.r / 255.0, target.g / 255.0, target.b / 255.0)});
        }
        reportProgress(callbacks,
                       kProgressScanEnd + static_cast<int>((kProgressGridEnd - kProgressScanEnd) *
                                                           (static_cast<double>(by + block) /
                                                            static_cast<double>(height))));
    }

    if (callbacks.status) {
        callbacks.status("Phase 3/3: Optimizing grid (spectral sort)");
    }
    std::stable_sort(grid_.begin(), grid_.end(),
                     [](const GridBlock& a, const GridBlock& b) { return a.hue < b.hue; });
}

void MosaicEngine::emitScanPreview(const FrameSink& sink, int scanY) const {
    // Visual language of the original: a dimmed copy of the input with the
    // scanned region at full brightness and a green outline at the scanline.
    QImage frame = dimmedInput_.copy();
    const int width = frame.width();
    const int height = frame.height();
    const int brightEnd = std::min(scanY, height);

    for (int y = 0; y < brightEnd; ++y) {
        std::memcpy(frame.scanLine(y), input_.constScanLine(y), pixelOffset(width));
    }

    const int block = settings_.blockSize;
    const auto setPixelGreen = [&frame, width](int x, int y) {
        if (x < 0 || x >= width || y < 0) {
            return;
        }
        uchar* row = frame.scanLine(y);
        std::memcpy(row + pixelOffset(x), kGreen.data(), kGreen.size());
    };

    // Horizontal scanline bands, 2 px thick, drawn inward from the rect.
    const int scanY2 = std::min(scanY + 1, height - 1);
    for (int y = scanY; y <= scanY2; ++y) {
        for (int x = 0; x < width; ++x) {
            setPixelGreen(x, y);
        }
    }
    const int rectBottom = std::min(scanY + block + 1, height - 1);
    for (int y = scanY + block; y <= rectBottom; ++y) {
        if (y < scanY) {
            continue;
        }
        for (int x = 0; x < width; ++x) {
            setPixelGreen(x, y);
        }
    }
    // Vertical outline edges between the two bands.
    const int outlineBottom = std::min(scanY + block + 1, height - 1);
    for (int y = scanY; y <= outlineBottom; ++y) {
        for (int x = 0; x < 2; ++x) {
            setPixelGreen(x, y);
            setPixelGreen(width - 1 - x, y);
        }
    }

    if (sink) {
        sink(frame, false);
    }
}

void MosaicEngine::animate(const EngineCallbacks& callbacks, const FrameSink& sink) {
    const std::size_t totalBlocks = grid_.size();
    if (totalBlocks == 0 || tiles_.empty()) {
        if (sink) {
            sink(canvas_, true);
        }
        return;
    }

    std::mt19937 rng(settings_.seed);
    const auto steps = static_cast<std::size_t>(settings_.animationSteps);
    const std::size_t blocksPerStep = std::max<std::size_t>(1, (totalBlocks + steps - 1) / steps);
    const std::ptrdiff_t jitterRange =
        std::max<std::ptrdiff_t>(1, static_cast<std::ptrdiff_t>(tiles_.size() / 200));
    const double blend = settings_.blendStrength;
    const int width = canvas_.width();
    const int height = canvas_.height();
    std::size_t processed = 0;

    for (int step = 0; step < settings_.animationSteps + kPaddingSteps; ++step) {
        if (cancelRequested(callbacks)) {
            return;
        }
        const std::size_t begin = processed;
        const std::size_t end = std::min(begin + blocksPerStep, totalBlocks);

        if (begin >= totalBlocks) {
            // Tail batches: the canvas is already complete.
            if (sink) {
                sink(canvas_, false);
            }
            continue;
        }

        for (std::size_t i = begin; i < end; ++i) {
            const GridBlock& block = grid_[i];

            // Select the tile whose luminance is closest from below, then
            // jitter around it, exactly like the original engine.
            auto it = std::lower_bound(tileLuminances_.begin(), tileLuminances_.end(),
                                       block.targetLuminance);
            std::ptrdiff_t idx = std::distance(tileLuminances_.begin(), it);
            std::uniform_int_distribution<std::ptrdiff_t> jitter(-jitterRange, jitterRange);
            idx = std::clamp<std::ptrdiff_t>(idx + jitter(rng), 0,
                                             static_cast<std::ptrdiff_t>(tiles_.size()) - 1);
            const Tile& tile = tiles_[static_cast<std::size_t>(idx)];

            // Blend the tile with the solid target colour and paste it.
            // For tiles clipped at the image edge, tile pixels are sampled
            // with nearest-neighbour scaling (the original resized with
            // bicubic; edge blocks only, visually negligible).
            for (int y = 0; y < block.h; ++y) {
                uchar* dst = canvas_.scanLine(block.y + y) + pixelOffset(block.x);
                const int sampleY = tile.h == 1 ? 0 : (y * tile.h) / block.h;
                const uchar* srcRow = input_.constScanLine(tile.y + sampleY) + pixelOffset(tile.x);
                for (int x = 0; x < block.w; ++x) {
                    const int sampleX = tile.w == 1 ? 0 : (x * tile.w) / block.w;
                    const uchar* s = srcRow + pixelOffset(sampleX);
                    dst[pixelOffset(x) + 0] =
                        static_cast<uchar>(std::lround(s[0] * (1.0 - blend) + block.tr * blend));
                    dst[pixelOffset(x) + 1] =
                        static_cast<uchar>(std::lround(s[1] * (1.0 - blend) + block.tg * blend));
                    dst[pixelOffset(x) + 2] =
                        static_cast<uchar>(std::lround(s[2] * (1.0 - blend) + block.tb * blend));
                }
            }
        }
        processed = end;

        // Shimmer: brighten the bounding box of the batch that was just
        // placed, giving the animation its characteristic sparkle.
        QImage frame = canvas_.copy();
        if (end > begin) {
            int minX = width;
            int minY = height;
            int maxX = 0;
            int maxY = 0;
            for (std::size_t i = begin; i < end; ++i) {
                minX = std::min(minX, grid_[i].x);
                minY = std::min(minY, grid_[i].y);
                maxX = std::max(maxX, grid_[i].x + grid_[i].w);
                maxY = std::max(maxY, grid_[i].y + grid_[i].h);
            }
            for (int y = minY; y < maxY; ++y) {
                uchar* row = frame.scanLine(y);
                for (int x = minX; x < maxX; ++x) {
                    uchar* p = row + pixelOffset(x);
                    p[0] = brighten(p[0], kShimmerFactor);
                    p[1] = brighten(p[1], kShimmerFactor);
                    p[2] = brighten(p[2], kShimmerFactor);
                }
            }
        }
        if (sink) {
            sink(frame, false);
        }

        reportProgress(
            callbacks,
            kProgressGridEnd +
                static_cast<int>((kProgressMax - kProgressGridEnd) *
                                 (static_cast<double>(step + 1) /
                                  static_cast<double>(settings_.animationSteps + kPaddingSteps))));
    }

    if (sink) {
        sink(canvas_, true);
    }
}

bool MosaicEngine::run(const EngineCallbacks& callbacks, const FrameSink& sink) {
    reportProgress(callbacks, 0);
    scanTiles(callbacks, sink);
    if (cancelRequested(callbacks)) {
        return false;
    }
    buildGrid(callbacks);
    if (cancelRequested(callbacks)) {
        return false;
    }
    animate(callbacks, sink);
    if (cancelRequested(callbacks)) {
        return false;
    }
    reportProgress(callbacks, kProgressMax);
    return true;
}

} // namespace itpixel
