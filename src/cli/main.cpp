#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QImage>
#include <QString>

#include "CliOptions.hpp"
#include "itpixel/GifExporter.hpp"
#include "itpixel/MosaicEngine.hpp"
#include "Paths.hpp"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitFailure = 1;
constexpr int kExitUsage = 2;

// One progress line per phase on stdout; a compact percent counter on
// stderr so scripts can capture clean output.
struct ConsoleCallbacks {
    int lastShownPercent = -1;

    void status(std::string_view text)
    {
        std::cout << text << '\n';
    }

    void progress(int percent)
    {
        if (percent == lastShownPercent) {
            return;
        }
        lastShownPercent = percent;
        std::cerr << "\rProgress: " << percent << "%" << std::flush;
        if (percent == 100) {
            std::cerr << '\n';
        }
    }
};

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const std::string programName = "it-pixel-cli";

    std::vector<std::string> args(argv + 1, argv + argc);
    auto parsed = itpixel::cli::CliOptions::parse(args);
    if (!parsed) {
        if (parsed.error() == "help") {
            std::cout << itpixel::cli::CliOptions::helpText(programName);
            return kExitOk;
        }
        std::cerr << "Error: " << parsed.error() << "\n\n"
                  << itpixel::cli::CliOptions::helpText(programName);
        return kExitUsage;
    }
    itpixel::cli::CliOptions options = std::move(*parsed);

    // Seed: explicit or random. Print it so any run can be reproduced.
    std::random_device device;
    const std::uint32_t seed =
        options.seed.value_or(static_cast<std::uint32_t>(device()));
    options.settings.seed = seed;

    const QString inputPath = QString::fromStdString(options.input.string());
    QString error;
    QImage input = itpixel::MosaicEngine::loadImage(inputPath, &error).value_or(QImage());
    if (input.isNull()) {
        std::cerr << "Error: " << error.toStdString() << '\n';
        return kExitFailure;
    }

    QString referencePath;
    if (options.reference.empty()) {
        referencePath = itpixel::defaultReferencePath();
        if (referencePath.isEmpty()) {
            std::cerr << "Error: no reference image given and the bundled asset could not "
                         "be found (expected assets/reference/it.jpeg)\n";
            return kExitFailure;
        }
    } else {
        referencePath = QString::fromStdString(options.reference.string());
    }
    QImage reference =
        itpixel::MosaicEngine::loadImage(referencePath, &error).value_or(QImage());
    if (reference.isNull()) {
        std::cerr << "Error: " << error.toStdString() << '\n';
        return kExitFailure;
    }

    std::cout << "Input:      " << options.input.string() << " (" << input.width() << "x"
              << input.height() << ")\n"
              << "Reference:  " << referencePath.toStdString() << " (resized to "
              << input.width() << "x" << input.height() << ")\n"
              << "Output:     " << options.output.string() << "\n"
              << "Seed:       " << seed << '\n';

    itpixel::MosaicEngine engine(std::move(input), std::move(reference), options.settings);
    if (engine.effectiveBlockSize() != options.settings.blockSize) {
        std::cout << "Note: block size raised to " << engine.effectiveBlockSize()
                  << " for this image size (block-count cap)\n";
    }

    std::string gifError;
    auto exporter = itpixel::GifExporter::create(
        options.output, engine.canvasSize(),
        itpixel::GifExporter::Options{options.fps, /*loopCount=*/0}, &gifError);
    if (!exporter) {
        std::cerr << "Error: " << gifError << '\n';
        return kExitFailure;
    }

    ConsoleCallbacks console;
    int framesWritten = 0;
    std::uint64_t frameIndex = 0;
    const int sample = options.frameSample;

    const itpixel::FrameSink sink = [&](const QImage& frame, bool isFinal) {
        // The final frame is always written so the GIF ends on the finished
        // mosaic even when sampling skips frames.
        if (!isFinal && frameIndex % static_cast<std::uint64_t>(sample) != 0) {
            ++frameIndex;
            return;
        }
        ++frameIndex;
        if (exporter->addFrame(frame)) {
            ++framesWritten;
        }
    };

    const bool completed = engine.run(
        itpixel::EngineCallbacks{
            [&](std::string_view text) { console.status(text); },
            [&](int percent) { console.progress(percent); },
            nullptr},
        sink);

    if (!completed) {
        std::cerr << "Error: transformation did not complete\n";
        return kExitFailure;
    }
    if (!exporter->finish()) {
        std::cerr << "Error: failed while closing the GIF file\n";
        return kExitFailure;
    }

    std::cout << "Done. " << framesWritten << " frames written to "
              << options.output.string() << '\n';
    return kExitOk;
}
