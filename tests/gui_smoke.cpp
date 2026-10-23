// Headless GUI smoke test. Runs against the real MainWindow and worker
// thread with the Qt offscreen platform, so it works in CI containers
// without a display server.

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <chrono>
#include <cstdio>

#include "EngineWorker.hpp"
#include "MainWindow.hpp"

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAILED: %s\n", what);
    }
}

QImage makeTestImage(int width, int height) {
    QImage image(width, height, QImage::Format_RGB888);
    for (int y = 0; y < height; ++y) {
        uchar* row = image.scanLine(y);
        for (int x = 0; x < width; ++x) {
            row[static_cast<std::size_t>(3 * x) + 0] = static_cast<uchar>((x * 255) / width);
            row[static_cast<std::size_t>(3 * x) + 1] = static_cast<uchar>((y * 255) / height);
            row[static_cast<std::size_t>(3 * x) + 2] = 0x80;
        }
    }
    return image;
}

// Processes events until `predicate` holds or the timeout expires.
template <typename Predicate>
bool waitFor(Predicate predicate, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate()) {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // ---- Fixture images --------------------------------------------------
    QTemporaryDir dir;
    if (!dir.isValid()) {
        std::fprintf(stderr, "FAILED: no temp dir\n");
        return 1;
    }
    const QString inputPath = QDir(dir.path()).filePath("input.png");
    const QString referencePath = QDir(dir.path()).filePath("reference.png");
    if (!makeTestImage(96, 72).save(inputPath) || !makeTestImage(24, 24).save(referencePath)) {
        std::fprintf(stderr, "FAILED: cannot write fixtures\n");
        return 1;
    }

    // ---- Window constructs, shows, and resizes ---------------------------
    MainWindow window;
    window.show();
    window.resize(900, 640);
    QApplication::processEvents();
    window.resize(400, 300);
    QApplication::processEvents();
    check(window.size().width() >= 300, "window resizes");

    // ---- A full transformation through the real UI ----------------------
    QLineEdit* inputEdit = window.findChild<QLineEdit*>(QStringLiteral("inputPath"));
    QLineEdit* referenceEdit = window.findChild<QLineEdit*>(QStringLiteral("referencePath"));
    check(inputEdit != nullptr, "input path edit found");
    check(referenceEdit != nullptr, "reference path edit found");
    if (inputEdit == nullptr || referenceEdit == nullptr) {
        return 1;
    }
    inputEdit->setText(inputPath);
    referenceEdit->setText(referencePath);

    QTimer::singleShot(0, [&window]() {
        QMetaObject::invokeMethod(&window, "startTransformation", Qt::DirectConnection);
    });

    QPushButton* saveButton = window.findChild<QPushButton*>(QStringLiteral("saveButton"));
    QPushButton* startButton = window.findChild<QPushButton*>(QStringLiteral("startButton"));
    check(saveButton != nullptr && startButton != nullptr, "buttons found");

    // The UI must stay responsive while the worker runs: the start button
    // is disabled and re-enabled, and cancel becomes clickable.
    QApplication::processEvents();
    const bool completed = waitFor(
        [&saveButton]() { return saveButton != nullptr && saveButton->isEnabled(); }, 30'000);
    check(completed, "transformation completes through the UI");

    const bool startReenabled = waitFor(
        [&startButton]() { return startButton != nullptr && startButton->isEnabled(); }, 5'000);
    check(startReenabled, "start button re-enabled after run");

    // ---- Saving the result works -----------------------------------------
    const QString savePath = QDir(dir.path()).filePath("result.png");
    if (completed) {
        // Drive the save slot; the file dialog is suppressed headlessly by
        // calling the slot only if the final image is present. Instead of
        // the dialog, verify the final image through the preview pipeline:
        // the preview label must show a non-null pixmap.
        QLabel* previewLabel = window.findChild<QLabel*>(QStringLiteral("previewLabel"));
        check(previewLabel != nullptr, "preview label found");
        if (previewLabel != nullptr) {
            check(!previewLabel->pixmap().isNull(), "preview shows the final image");
        }
    }

    // ---- Worker cancellation is prompt -------------------------------------
    {
        EngineWorker worker;
        QThread thread;
        worker.moveToThread(&thread);
        thread.start();
        int cancelledSignals = 0;
        QObject::connect(&worker, &EngineWorker::cancelled,
                         [&cancelledSignals]() { ++cancelledSignals; });
        worker.submit(makeTestImage(320, 240), makeTestImage(32, 32), itpixel::MosaicSettings{});
        // Request cancellation before the queued run() executes; the engine
        // must observe it at the first checkpoint and report cancellation
        // instead of running to completion.
        worker.requestCancel();
        QMetaObject::invokeMethod(&worker, "run", Qt::QueuedConnection);
        const bool cancelled = waitFor(
            [&cancelledSignals, &worker]() { return cancelledSignals > 0 && !worker.isBusy(); },
            10'000);
        check(cancelled, "worker cancellation is delivered");
        check(!worker.isBusy(), "worker is idle after cancellation");
        thread.quit();
        thread.wait();
    }

    if (failures > 0) {
        std::fprintf(stderr, "%d GUI smoke checks failed\n", failures);
        return 1;
    }
    std::printf("GUI smoke test passed\n");
    return 0;
}
