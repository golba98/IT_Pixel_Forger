#include "MainWindow.hpp"

#include <algorithm>

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>

#include "EngineWorker.hpp"
#include "Paths.hpp"
#include "itpixel/MosaicEngine.hpp"

namespace {

constexpr int kDefaultSteps = 120;
constexpr int kMaxSteps = 10'000;
constexpr int kMaxBlock = 1'024;

const QColor kPreviewBackground = QColor(0x11, 0x11, 0x11);

QString imageFilter() {
    return QStringLiteral("Images (*.jpg *.jpeg *.png *.webp *.bmp *.gif);;All files (*)");
}

QString formatDuration(qint64 ms) {
    if (ms < 0) {
        return QStringLiteral("--");
    }
    if (ms < 1000) {
        return QStringLiteral("%1 ms").arg(ms);
    }
    if (ms < 60'000) {
        return QStringLiteral("%1 s").arg(
            QString::number(static_cast<double>(ms) / 1000.0, 'f', 1));
    }
    return QStringLiteral("%1 m %2 s").arg(ms / 60'000).arg((ms / 1'000) % 60);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), defaultReferencePath_(itpixel::defaultReferencePath()) {
    buildUi();

    worker_ = new EngineWorker;
    worker_->moveToThread(&workerThread_);
    connect(&workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(this, &MainWindow::destroyed, worker_, &EngineWorker::requestCancel);
    workerThread_.start();

    connect(worker_, &EngineWorker::statusChanged, this, &MainWindow::onWorkerStatus);
    connect(worker_, &EngineWorker::progressChanged, this, &MainWindow::onWorkerProgress);
    connect(worker_, &EngineWorker::previewReady, this, &MainWindow::onWorkerPreview);
    connect(worker_, &EngineWorker::finished, this, &MainWindow::onWorkerFinished);
    connect(worker_, &EngineWorker::cancelled, this, &MainWindow::onWorkerCancelled);
    connect(worker_, &EngineWorker::failed, this, &MainWindow::onWorkerFailed);

    referencePathEdit_->setText(defaultReferencePath_);
    setWindowTitle(QStringLiteral("IT Pixel Forger"));
    resize(1180, 760);
    statusBar()->hide();
}

MainWindow::~MainWindow() {
    workerThread_.quit();
    workerThread_.wait();
}

void MainWindow::buildUi() {
    // ---- Left column: files and settings -------------------------------
    auto* filesGroup = new QGroupBox(tr("Images"), this);
    auto* filesLayout = new QFormLayout(filesGroup);
    filesLayout->setLabelAlignment(Qt::AlignRight);

    inputPathEdit_ = new QLineEdit(this);
    inputPathEdit_->setObjectName(QStringLiteral("inputPath"));
    auto* inputBrowse = new QPushButton(tr("Browse..."), this);
    inputBrowse->setFixedWidth(96);
    connect(inputBrowse, &QPushButton::clicked, this, &MainWindow::browseInput);
    auto* inputRow = new QHBoxLayout();
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->addWidget(inputPathEdit_, 1);
    inputRow->addWidget(inputBrowse);
    filesLayout->addRow(tr("Input"), inputRow);

    referencePathEdit_ = new QLineEdit(this);
    referencePathEdit_->setObjectName(QStringLiteral("referencePath"));
    auto* referenceBrowse = new QPushButton(tr("Browse..."), this);
    referenceBrowse->setFixedWidth(96);
    connect(referenceBrowse, &QPushButton::clicked, this, &MainWindow::browseReference);
    auto* referenceRow = new QHBoxLayout();
    referenceRow->setContentsMargins(0, 0, 0, 0);
    referenceRow->addWidget(referencePathEdit_, 1);
    referenceRow->addWidget(referenceBrowse);
    filesLayout->addRow(tr("Reference"), referenceRow);

    auto* settingsGroup = new QGroupBox(tr("Transformation settings"), this);
    auto* settingsLayout = new QFormLayout(settingsGroup);
    settingsLayout->setLabelAlignment(Qt::AlignRight);

    blockSizeSpin_ = new QSpinBox(this);
    blockSizeSpin_->setRange(1, kMaxBlock);
    blockSizeSpin_->setValue(1);
    blockSizeSpin_->setSuffix(QStringLiteral(" px"));
    blockSizeSpin_->setToolTip(tr("Edge length of one mosaic block."));
    settingsLayout->addRow(tr("Block size"), blockSizeSpin_);

    stepsSpin_ = new QSpinBox(this);
    stepsSpin_->setRange(1, kMaxSteps);
    stepsSpin_->setValue(kDefaultSteps);
    stepsSpin_->setToolTip(tr("Number of animation batches used to reveal the mosaic."));
    settingsLayout->addRow(tr("Steps"), stepsSpin_);

    blendSlider_ = new QSlider(Qt::Horizontal, this);
    blendSlider_->setRange(0, 100);
    blendSlider_->setValue(100);
    blendSlider_->setToolTip(tr("How strongly each block takes the reference colour.\n"
                                "0 keeps the sampled tile, 1 uses the plain target colour."));
    blendValueLabel_ = new QLabel(QStringLiteral("100%"), this);
    blendValueLabel_->setFixedWidth(48);
    connect(blendSlider_, &QSlider::valueChanged, this, &MainWindow::onBlendChanged);
    auto* blendRow = new QHBoxLayout();
    blendRow->setContentsMargins(0, 0, 0, 0);
    blendRow->addWidget(blendSlider_, 1);
    blendRow->addWidget(blendValueLabel_);
    settingsLayout->addRow(tr("Colour blend"), blendRow);

    seedEdit_ = new QLineEdit(this);
    seedEdit_->setPlaceholderText(tr("random"));
    seedEdit_->setToolTip(tr("Leave empty for a random seed each run; enter a number "
                             "for reproducible transformations."));
    settingsLayout->addRow(tr("Seed"), seedEdit_);

    // ---- Actions --------------------------------------------------------
    auto* actionsLayout = new QVBoxLayout();
    startButton_ = new QPushButton(tr("Start transformation"), this);
    startButton_->setObjectName(QStringLiteral("startButton"));
    startButton_->setMinimumHeight(40);
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::startTransformation);
    actionsLayout->addWidget(startButton_);

    cancelButton_ = new QPushButton(tr("Cancel"), this);
    cancelButton_->setMinimumHeight(32);
    cancelButton_->setEnabled(false);
    connect(cancelButton_, &QPushButton::clicked, this, &MainWindow::cancelTransformation);
    actionsLayout->addWidget(cancelButton_);

    saveButton_ = new QPushButton(tr("Save image..."), this);
    saveButton_->setObjectName(QStringLiteral("saveButton"));
    saveButton_->setMinimumHeight(32);
    saveButton_->setEnabled(false);
    connect(saveButton_, &QPushButton::clicked, this, &MainWindow::saveResult);
    actionsLayout->addWidget(saveButton_);
    actionsLayout->addStretch(1);

    auto* leftColumn = new QVBoxLayout();
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->addWidget(filesGroup);
    leftColumn->addWidget(settingsGroup);
    leftColumn->addLayout(actionsLayout);
    leftColumn->addStretch(1);
    auto* leftWidget = new QWidget(this);
    leftWidget->setLayout(leftColumn);
    leftWidget->setFixedWidth(340);

    // ---- Preview --------------------------------------------------------
    previewArea_ = new QWidget(this);
    previewArea_->setAutoFillBackground(true);
    QPalette areaPalette = previewArea_->palette();
    areaPalette.setColor(QPalette::Window, kPreviewBackground);
    previewArea_->setPalette(areaPalette);

    previewLabel_ = new QLabel(previewArea_);
    previewLabel_->setObjectName(QStringLiteral("previewLabel"));
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setMinimumSize(320, 240);
    previewLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    // ---- Status ---------------------------------------------------------
    progressBar_ = new QProgressBar(this);
    progressBar_->setTextVisible(false);
    progressBar_->setMaximum(100);
    progressBar_->setFixedHeight(6);
    statusLabel_ = new QLabel(tr("Ready."), this);
    statusLabel_->setMinimumWidth(200);
    timeLabel_ = new QLabel(this);
    timeLabel_->setAlignment(Qt::AlignRight);
    auto* statusRow = new QHBoxLayout();
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->addWidget(statusLabel_, 1);
    statusRow->addWidget(timeLabel_);

    auto* rightColumn = new QVBoxLayout();
    rightColumn->setContentsMargins(0, 0, 0, 0);
    rightColumn->setSpacing(8);
    rightColumn->addWidget(previewArea_, 1);
    rightColumn->addWidget(progressBar_);
    rightColumn->addLayout(statusRow);
    auto* rightWidget = new QWidget(this);
    rightWidget->setLayout(rightColumn);

    auto* centralLayout = new QHBoxLayout();
    centralLayout->setContentsMargins(16, 16, 16, 16);
    centralLayout->setSpacing(16);
    centralLayout->addWidget(leftWidget);
    centralLayout->addWidget(rightWidget, 1);
    auto* central = new QWidget(this);
    central->setLayout(centralLayout);
    setCentralWidget(central);
}

void MainWindow::browseInput() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select input image"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), imageFilter());
    if (path.isEmpty()) {
        return;
    }
    inputPathEdit_->setText(path);
    loadPreviewOfInput();
}

void MainWindow::browseReference() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select reference image"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation), imageFilter());
    if (!path.isEmpty()) {
        referencePathEdit_->setText(path);
    }
}

void MainWindow::loadPreviewOfInput() {
    QString error;
    const auto image = itpixel::MosaicEngine::loadImage(inputPathEdit_->text(), &error);
    if (!image.has_value()) {
        QMessageBox::warning(this, tr("Image error"), error);
        return;
    }
    showImage(*image);
    statusLabel_->setText(tr("Loaded %1 (%2x%3)")
                              .arg(QFileInfo(inputPathEdit_->text()).fileName())
                              .arg(image->width())
                              .arg(image->height()));
}

void MainWindow::startTransformation() {
    if (running_) {
        return;
    }
    const QString inputPath = inputPathEdit_->text();
    const QString referencePath = referencePathEdit_->text();

    if (inputPath.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("Missing input"), tr("Please choose an input image first."));
        return;
    }
    if (!QFile::exists(inputPath)) {
        QMessageBox::warning(this, tr("Missing input"),
                             tr("The input image does not exist:\n%1").arg(inputPath));
        return;
    }
    if (referencePath.trimmed().isEmpty() || !QFile::exists(referencePath)) {
        QMessageBox::warning(this, tr("Missing reference"),
                             tr("The reference image does not exist:\n%1").arg(referencePath));
        return;
    }

    QString error;
    auto input = itpixel::MosaicEngine::loadImage(inputPath, &error);
    if (!input.has_value()) {
        QMessageBox::warning(this, tr("Image error"),
                             tr("Could not load the input image:\n%1").arg(error));
        return;
    }
    auto reference = itpixel::MosaicEngine::loadImage(referencePath, &error);
    if (!reference.has_value()) {
        QMessageBox::warning(this, tr("Image error"),
                             tr("Could not load the reference image:\n%1").arg(error));
        return;
    }

    itpixel::MosaicSettings settings = settingsFromUi();
    finalImage_ = QImage();
    setRunning(true);
    showImage(*input);

    worker_->submit(std::move(*input), std::move(*reference), settings);
    QMetaObject::invokeMethod(worker_, &EngineWorker::run, Qt::QueuedConnection);
}

void MainWindow::cancelTransformation() {
    if (running_) {
        worker_->requestCancel();
        cancelButton_->setEnabled(false);
        statusLabel_->setText(tr("Cancelling..."));
    }
}

void MainWindow::saveResult() {
    if (finalImage_.isNull()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save image"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
            QStringLiteral("/it-pixel-result"),
        QStringLiteral("PNG (*.png);;JPEG (*.jpg *.jpeg)"));
    if (path.isEmpty()) {
        return;
    }
    const QByteArray format = path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)
                                  ? QByteArrayLiteral("PNG")
                                  : QByteArrayLiteral("JPEG");
    if (!finalImage_.save(path, format.constData(), 95)) {
        QMessageBox::warning(this, tr("Save error"),
                             tr("Could not save the image to:\n%1").arg(path));
        return;
    }
    statusLabel_->setText(tr("Saved %1").arg(QDir::toNativeSeparators(path)));
}

void MainWindow::onWorkerStatus(const QString& status) {
    statusLabel_->setText(status);
}

void MainWindow::onWorkerProgress(int percent, qint64 elapsedMs, qint64 etaMs) {
    progressBar_->setValue(percent);
    QString time = QStringLiteral("Elapsed %1").arg(formatDuration(elapsedMs));
    if (etaMs >= 0) {
        time += QStringLiteral("   |   Remaining ~%1").arg(formatDuration(etaMs));
    }
    timeLabel_->setText(time);
}

void MainWindow::onWorkerPreview(const QImage& preview) {
    if (finalImage_.isNull()) {
        showImage(preview);
    }
}

void MainWindow::onWorkerFinished(const QImage& finalImage) {
    finalImage_ = finalImage;
    showImage(finalImage);
    progressBar_->setValue(100);
    statusLabel_->setText(tr("Transformation complete."));
    timeLabel_->setText(tr("Done"));
    setRunning(false);
}

void MainWindow::onWorkerCancelled() {
    statusLabel_->setText(tr("Cancelled."));
    timeLabel_->clear();
    setRunning(false);
}

void MainWindow::onWorkerFailed(const QString& message) {
    QMessageBox::warning(this, tr("Transformation error"), message);
    statusLabel_->setText(tr("Failed."));
    timeLabel_->clear();
    setRunning(false);
}

void MainWindow::onBlendChanged(int value) {
    blendValueLabel_->setText(QStringLiteral("%1%").arg(value));
}

void MainWindow::showImage(const QImage& image) {
    previewImage_ = image;
    refreshPreview();
}

void MainWindow::refreshPreview() {
    if (previewImage_.isNull()) {
        previewLabel_->setPixmap(QPixmap());
        return;
    }
    const QRect available = previewArea_->contentsRect();
    if (available.width() < 4 || available.height() < 4) {
        return;
    }
    QPixmap pixmap = QPixmap::fromImage(previewImage_);
    pixmap = pixmap.scaled(available.width(), available.height(), Qt::KeepAspectRatio,
                           Qt::SmoothTransformation);
    previewLabel_->setPixmap(pixmap);
    // Center the label inside the preview area.
    previewLabel_->setGeometry(QRect(QPoint((available.width() - pixmap.width()) / 2,
                                            (available.height() - pixmap.height()) / 2),
                                     pixmap.size()));
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    refreshPreview();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    worker_->requestCancel();
    workerThread_.quit();
    if (!workerThread_.wait(5'000)) {
        workerThread_.terminate();
        workerThread_.wait();
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::setRunning(bool running) {
    running_ = running;
    startButton_->setEnabled(!running);
    cancelButton_->setEnabled(running);
    saveButton_->setEnabled(!running && !finalImage_.isNull());
    inputPathEdit_->setEnabled(!running);
    referencePathEdit_->setEnabled(!running);
    blockSizeSpin_->setEnabled(!running);
    stepsSpin_->setEnabled(!running);
    blendSlider_->setEnabled(!running);
    seedEdit_->setEnabled(!running);
    if (running) {
        progressBar_->setValue(0);
        timeLabel_->clear();
    }
}

itpixel::MosaicSettings MainWindow::settingsFromUi() const {
    itpixel::MosaicSettings settings;
    settings.blockSize = blockSizeSpin_->value();
    settings.animationSteps = stepsSpin_->value();
    settings.blendStrength = blendSlider_->value() / 100.0;
    settings.blendStrength = std::clamp(settings.blendStrength, 0.0, 1.0);
    const QString seedText = seedEdit_->text().trimmed();
    if (!seedText.isEmpty()) {
        bool ok = false;
        const qulonglong seed = seedText.toULongLong(&ok);
        settings.seed = ok ? static_cast<std::uint32_t>(seed % 4'294'967'296ULL) : 0;
    } else {
        settings.seed = static_cast<std::uint32_t>(QRandomGenerator::global()->generate());
    }
    return settings;
}
