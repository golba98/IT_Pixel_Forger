#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

#include <QImage>
#include <QSize>
#include <QString>

#include "itpixel/MosaicSettings.hpp"
#include "itpixel/Tile.hpp"

namespace itpixel {

// Errors are reported as user-facing messages instead of exceptions so both
// the CLI and the GUI can present them directly.
using LoadResult = std::optional<QImage>;

// Errors are reported as user-facing messages instead of exceptions so both
// the CLI and the GUI can present them directly.
using LoadResult = std::optional<QImage>;

struct EngineCallbacks {
    // Phase descriptions ("Phase 1/3: ..."), called rarely.
    std::function<void(std::string_view)> status;
    // Overall run progress in [0, 100], called frequently.
    std::function<void(int)> progress;
    // Return true to request cancellation. Checked between batches.
    std::function<bool()> cancelled;
};

// Receives every produced frame (scan previews and animation frames).
// `isFinal` marks the last frame, which is the completed mosaic.
using FrameSink = std::function<void(const QImage& frame, bool isFinal)>;

// Reimplementation of the original mosaic engine. The input image is cut into
// blocks ("tiles"); a second grid of blocks over the reference image drives
// which tile is placed where. Blocks are revealed in reference-hue order,
// each block being filled with the input tile whose average luminance best
// matches the reference block, optionally blended toward the reference colour.
//
// The class is single-threaded; callers that need responsiveness run it on a
// worker thread and communicate through EngineCallbacks (see EngineWorker).
class MosaicEngine {
public:
    // Loads an image from disk and converts it to RGB888. Returns nullopt
    // with `error` set on failure.
    [[nodiscard]] static LoadResult loadImage(const QString& path, QString* error = nullptr);

    // Resizes an image with a high-quality smooth filter (the original used
    // Pillow's LANCZOS; Qt's SmoothTransformation is its practical equivalent).
    [[nodiscard]] static QImage scaled(const QImage& image, QSize size);

    // Block size that would be used for a given canvas size. The original
    // implementation only adjusted the block size above 60 million pixels,
    // which almost never triggered; work is driven by the block count, so
    // that is what gets capped here.
    [[nodiscard]] static int effectiveBlockSizeFor(QSize size, int requestedBlock);

    // Both images must be non-null. Throws std::invalid_argument otherwise.
    // `settings` are validated internally.
    MosaicEngine(QImage input, QImage reference, MosaicSettings settings);

    // Block size after the large-image policy was applied.
    [[nodiscard]] int effectiveBlockSize() const { return settings_.blockSize; }
    [[nodiscard]] QSize canvasSize() const { return input_.size(); }

    // Runs preparation (tile extraction, grid analysis) and the animation,
    // passing each frame to `sink`. Returns false if cancelled.
    bool run(const EngineCallbacks& callbacks, const FrameSink& sink);

private:
    struct GridBlock {
        int x{};
        int y{};
        int w{};
        int h{};
        std::uint8_t tr{};
        std::uint8_t tg{};
        std::uint8_t tb{};
        double targetLuminance{};
        double hue{};
    };

    void scanTiles(const EngineCallbacks& callbacks, const FrameSink& sink);
    void buildGrid(const EngineCallbacks& callbacks);
    void animate(const EngineCallbacks& callbacks, const FrameSink& sink);
    [[nodiscard]] bool cancelRequested(const EngineCallbacks& callbacks) const;
    void reportProgress(const EngineCallbacks& callbacks, int progress) const;
    void emitScanPreview(const FrameSink& sink, int scanY) const;

    QImage input_;        // RGB888, the mosaic source material
    QImage reference_;    // RGB888, resized to input size
    QImage canvas_;       // working image, becomes the final mosaic
    QImage dimmedInput_;  // brightness 0.3 copy for scan previews
    MosaicSettings settings_;

    std::vector<Tile> tiles_;        // sorted by luminance (stable)
    std::vector<double> tileLuminances_;  // parallel to tiles_ for binary search
    std::vector<GridBlock> grid_;    // sorted by hue (stable)
};

}  // namespace itpixel
