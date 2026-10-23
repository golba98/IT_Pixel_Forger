#pragma once

#include <QImage>
#include <QLabel>
#include <QMainWindow>
#include <QThread>

#include "itpixel/MosaicSettings.hpp"

class EngineWorker;
class QCheckBox;
class QFileDialog;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QSpinBox;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void browseInput();
    void browseReference();
    void startTransformation();
    void cancelTransformation();
    void saveResult();
    void onWorkerStatus(const QString& status);
    void onWorkerProgress(int percent, qint64 elapsedMs, qint64 etaMs);
    void onWorkerPreview(const QImage& preview);
    void onWorkerFinished(const QImage& finalImage);
    void onWorkerCancelled();
    void onWorkerFailed(const QString& message);
    void onBlendChanged(int value);

private:
    void buildUi();
    void showImage(const QImage& image);
    void refreshPreview();
    void setRunning(bool running);
    void loadPreviewOfInput();
    [[nodiscard]] itpixel::MosaicSettings settingsFromUi() const;

    // Files and settings
    QLineEdit* inputPathEdit_{};
    QLineEdit* referencePathEdit_{};
    QSpinBox* blockSizeSpin_{};
    QSpinBox* stepsSpin_{};
    QSlider* blendSlider_{};
    QLabel* blendValueLabel_{};
    QLineEdit* seedEdit_{};

    // Actions
    QPushButton* startButton_{};
    QPushButton* cancelButton_{};
    QPushButton* saveButton_{};

    // Preview and status
    QWidget* previewArea_{};
    QLabel* previewLabel_{};
    QProgressBar* progressBar_{};
    QLabel* statusLabel_{};
    QLabel* timeLabel_{};

    // Worker
    QThread workerThread_;
    EngineWorker* worker_{};
    QString defaultReferencePath_;

    QImage previewImage_;   // downscaled frame used for display
    QImage finalImage_;     // full-resolution result
    bool running_{false};
};
