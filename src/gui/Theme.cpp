#include "Theme.hpp"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>

namespace itpixel::gui {

void applyDarkTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    const QColor window(0x24, 0x26, 0x2a);
    const QColor surface(0x2e, 0x31, 0x36);
    const QColor text(0xe8, 0xe8, 0xe8);
    const QColor disabledText(0x88, 0x88, 0x88);
    const QColor accent(0xd6, 0x28, 0x28);
    const QColor highlight(0x45, 0x48, 0x50);

    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledText);
    palette.setColor(QPalette::Base, surface);
    palette.setColor(QPalette::AlternateBase, window);
    palette.setColor(QPalette::ToolTipBase, surface);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledText);
    palette.setColor(QPalette::Button, surface);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledText);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Highlight, highlight);
    palette.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9a, 0x9a));
    palette.setColor(QPalette::Link, accent);
    app.setPalette(palette);

    app.setStyleSheet(QStringLiteral(R"(
        QGroupBox {
            font-weight: 600;
            border: 1px solid #3a3d42;
            border-radius: 6px;
            margin-top: 12px;
            padding: 10px 8px 8px 8px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
        }
        QPushButton#startButton {
            background-color: #d62828;
            color: white;
            font-weight: 600;
            border: none;
            border-radius: 6px;
        }
        QPushButton#startButton:hover { background-color: #e03b3b; }
        QPushButton#startButton:pressed { background-color: #b52222; }
        QPushButton#startButton:disabled { background-color: #4a3234; color: #8a7f7f; }
        QLineEdit, QSpinBox {
            background-color: #24262a;
            border: 1px solid #3a3d42;
            border-radius: 4px;
            padding: 4px 6px;
        }
        QProgressBar {
            background-color: #2e3136;
            border: none;
            border-radius: 3px;
        }
        QProgressBar::chunk {
            background-color: #d62828;
            border-radius: 3px;
        }
    )"));
}

} // namespace itpixel::gui
