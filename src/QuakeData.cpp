#include "QuakeData.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

PakValidation validatePaks(const QString &quakeDir)
{
    const QDir id1(QDir(quakeDir).filePath("id1"));
    return {
        id1.exists(),
        QFileInfo::exists(id1.filePath("pak0.pak")),
        QFileInfo::exists(id1.filePath("pak1.pak")),
        QFileInfo::exists(id1.filePath("PAK0.PAK")),
        QFileInfo::exists(id1.filePath("PAK1.PAK")),
    };
}

bool createLowercasePakSymlinks(const QString &quakeDir)
{
#ifdef Q_OS_LINUX
    const QDir id1(QDir(quakeDir).filePath("id1"));
    bool ok = true;
    if (!QFileInfo::exists(id1.filePath("pak0.pak"))) {
        ok = QFile::link("PAK0.PAK", id1.filePath("pak0.pak")) && ok;
    }
    if (!QFileInfo::exists(id1.filePath("pak1.pak"))) {
        ok = QFile::link("PAK1.PAK", id1.filePath("pak1.pak")) && ok;
    }
    return ok;
#else
    Q_UNUSED(quakeDir);
    return false;
#endif
}
