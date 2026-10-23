#include "Paths.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

namespace itpixel {

QString defaultReferencePath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("../assets/reference/it.jpeg")),   // build tree
        QDir(appDir).filePath(QStringLiteral("assets/reference/it.jpeg")),      // flat layout
        QDir(appDir).filePath(QStringLiteral("../share/itpixel/it.jpeg")),      // installed
        QDir::current().filePath(QStringLiteral("assets/reference/it.jpeg")),    // repo root
    };
    for (const QString& candidate : candidates) {
        if (QFile::exists(candidate)) {
            return QDir::cleanPath(candidate);
        }
    }
    return {};
}

}  // namespace itpixel
