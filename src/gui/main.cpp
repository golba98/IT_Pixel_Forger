#include <QApplication>
#include <QCommandLineParser>

#include "MainWindow.hpp"
#include "Theme.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("IT Pixel Forger"));
    QApplication::setApplicationVersion(QStringLiteral("2.0.0"));
    QApplication::setOrganizationName(QStringLiteral("IT Pixel Forger"));

    itpixel::gui::applyDarkTheme(app);

    MainWindow window;
    window.show();
    return app.exec();
}
