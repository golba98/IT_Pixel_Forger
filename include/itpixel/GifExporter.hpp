#pragma once

#include <QImage>
#include <QSize>
#include <cstddef>
#include <filesystem>

namespace itpixel {

// Streaming animated-GIF writer built on the vendored giflib (MIT).
// Frames are quantized to a per-frame 256-colour adaptive palette with a
// local colour table, written one at a time so memory stays bounded
// regardless of animation length.
class GifExporter {
public:
    struct Options {
        int fps{25};      // playback speed; delay = round(100 / fps) centiseconds
        int loopCount{0}; // 0 = loop forever (GIF NETSCAPE extension)
    };

    // Returns nullopt with `error` set on failure.
    [[nodiscard]] static std::optional<GifExporter> create(const std::filesystem::path& path,
                                                           QSize size, Options options,
                                                           std::string* error = nullptr);

    GifExporter(GifExporter&& other) noexcept;
    GifExporter& operator=(GifExporter&& other) noexcept;
    GifExporter(const GifExporter&) = delete;
    GifExporter& operator=(const GifExporter&) = delete;

    // Quantizes and appends one frame. Returns false on write failure;
    // the object is still closable via finish().
    bool addFrame(const QImage& frame);

    // Completes the file and closes it. Returns false on failure; safe to
    // call once. The destructor finishes the file if finish() was not called.
    bool finish();

    ~GifExporter();

private:
    GifExporter() = default;
    struct Impl;
    Impl* impl_{};
};

} // namespace itpixel
