#pragma once

#include <QImage>
#include <QObject>
#include <atomic>

#include "itpixel/MosaicEngine.hpp"

// Runs the mosaic engine on a dedicated thread. The engine never touches the
// GUI; results are delivered through queued signals.
class EngineWorker : public QObject {
    Q_OBJECT

public:
    explicit EngineWorker(QObject* parent = nullptr);

    // Hands the next job to the worker. Must only be called while idle.
    void submit(QImage input, QImage reference, itpixel::MosaicSettings settings);

    // Asynchronous; the engine checks the flag between work batches.
    void requestCancel() { cancelRequested_ = true; }
    [[nodiscard]] bool isBusy() const { return busy_.load(); }

public slots:
    void run();

signals:
    void statusChanged(const QString& status);
    void progressChanged(int percent, qint64 elapsedMs, qint64 etaMs);
    void previewReady(const QImage& preview);
    void finished(const QImage& finalImage);
    void cancelled();
    void failed(const QString& message);

private:
    QImage input_;
    QImage reference_;
    itpixel::MosaicSettings settings_;
    std::atomic_bool busy_{false};
    std::atomic_bool cancelRequested_{false};
};
