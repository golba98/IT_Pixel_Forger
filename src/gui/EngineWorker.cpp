#include "EngineWorker.hpp"

#include <QElapsedTimer>

namespace {

// Minimum time between emitted preview frames; keeps the GUI event loop
// free while the engine produces frames as fast as it can.
constexpr qint64 kPreviewIntervalMs = 50;
constexpr int kPreviewMaxDimension = 1280;

QImage scalePreview(const QImage& frame) {
    if (frame.width() <= kPreviewMaxDimension && frame.height() <= kPreviewMaxDimension) {
        return frame.copy();
    }
    return frame.scaled(kPreviewMaxDimension, kPreviewMaxDimension, Qt::KeepAspectRatio,
                        Qt::SmoothTransformation);
}

} // namespace

EngineWorker::EngineWorker(QObject* parent) : QObject(parent) {}

void EngineWorker::submit(QImage input, QImage reference, itpixel::MosaicSettings settings) {
    input_ = std::move(input);
    reference_ = std::move(reference);
    settings_ = settings;
    // Cleared here, not in run(): a cancellation requested before the queued
    // run() executes must stay in effect.
    cancelRequested_.store(false);
}

void EngineWorker::run() {
    if (busy_.load()) {
        return;
    }
    busy_.store(true);

    try {
        itpixel::MosaicEngine engine(std::move(input_), std::move(reference_), settings_);
        QElapsedTimer timer;
        timer.start();
        qint64 lastPreviewAt = -kPreviewIntervalMs; // always show the first frame
        int lastPercent = -1;

        const itpixel::FrameSink sink = [&](const QImage& frame, bool isFinal) {
            if (isFinal) {
                emit finished(frame.copy());
                return;
            }
            const qint64 now = timer.elapsed();
            if (now - lastPreviewAt >= kPreviewIntervalMs) {
                lastPreviewAt = now;
                emit previewReady(scalePreview(frame));
            }
        };

        const itpixel::EngineCallbacks callbacks{
            .status =
                [&](std::string_view text) {
                    emit statusChanged(
                        QString::fromUtf8(text.data(), static_cast<int>(text.size())));
                },
            .progress =
                [&](int percent) {
                    if (percent == lastPercent) {
                        return;
                    }
                    lastPercent = percent;
                    const qint64 elapsed = timer.elapsed();
                    qint64 eta = -1; // unknown until progress starts
                    if (percent > 0) {
                        eta = elapsed * (100 - percent) / percent;
                    }
                    emit progressChanged(percent, elapsed, eta);
                },
            .cancelled = [&]() { return cancelRequested_.load(); }};

        if (!engine.run(callbacks, sink)) {
            emit cancelled();
        }
    } catch (const std::exception& e) {
        emit failed(QString::fromUtf8(e.what()));
    } catch (...) {
        emit failed(QStringLiteral("Unexpected error during transformation."));
    }
    busy_.store(false);
}
