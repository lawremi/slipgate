#include "InstallManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr auto AppId = "slipgate";

QString normalizedRelative(QString value)
{
    value.replace('\\', '/');
    while (value.startsWith('/')) {
        value.remove(0, 1);
    }
    value = QDir::cleanPath(value);
    if (value == ".") {
        return {};
    }
    return value;
}

bool safeChildPath(const QString &root, const QString &relative, QString *out)
{
    const QString rootCanonical = QFileInfo(root).canonicalFilePath();
    if (rootCanonical.isEmpty()) {
        return false;
    }

    const QString combined = QDir(rootCanonical).filePath(normalizedRelative(relative));
    const QString clean = QDir::cleanPath(combined);
    if (clean != rootCanonical && !clean.startsWith(rootCanonical + '/')) {
        return false;
    }

    *out = clean;
    return true;
}

} // namespace

QString appDataDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + "/.local/share/" + AppId;
    }
    return dir;
}

QString downloadsDir()
{
    return appDataDir() + "/downloads";
}

QString manifestPathFor(const ModEntry &entry)
{
    return appDataDir() + "/installed/" + entry.sha256 + ".json";
}

QString archivePathFor(const ModEntry &entry)
{
    QString filename = entry.filename;
    if (filename.isEmpty() && !entry.urls.isEmpty()) {
        filename = QFileInfo(QUrl(entry.urls.front()).path()).fileName();
    }
    if (filename.isEmpty()) {
        filename = entry.sha256 + ".zip";
    }
    return downloadsDir() + '/' + filename;
}

bool verifySha256(const QString &path, const QString &expected)
{
    if (expected.isEmpty()) {
        return true;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return false;
    }
    return QString::fromLatin1(hash.result().toHex()).compare(expected, Qt::CaseInsensitive) == 0;
}

InstallState loadInstallManifests()
{
    InstallState state;
    const QString dir = appDataDir() + "/installed";
    QDirIterator it(dir, {"*.json"}, QDir::Files);
    while (it.hasNext()) {
        QFile file(it.next());
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const QJsonObject object = doc.object();
        const QString sha = object.value("sha256").toString();
        if (sha.isEmpty()) {
            continue;
        }

        QStringList files;
        for (const QJsonValue &value : object.value("files").toArray()) {
            files << value.toString();
        }

        state.installedSha.insert(sha);
        state.installedAt.insert(sha, object.value("installed_at").toString());
        state.installedFiles.insert(sha, files);
    }
    return state;
}

void saveInstallManifest(const ModEntry &entry, const QStringList &files, InstallState *state)
{
    QDir().mkpath(appDataDir() + "/installed");
    const QString installedAt = QDateTime::currentDateTime().toString("MMM d, yyyy h:mm AP");

    QJsonArray fileArray;
    for (const QString &file : files) {
        fileArray << file;
    }

    QJsonObject object;
    object.insert("sha256", entry.sha256);
    object.insert("title", entry.displayTitle());
    object.insert("installed_at", installedAt);
    object.insert("files", fileArray);

    QSaveFile file(manifestPathFor(entry));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        file.commit();
    }

    if (state) {
        state->installedSha.insert(entry.sha256);
        state->installedAt.insert(entry.sha256, installedAt);
        state->installedFiles.insert(entry.sha256, files);
    }
}

bool extractZip(const QString &archivePath, const QString &destinationDir, QString *error)
{
    QString program = QStandardPaths::findExecutable("bsdtar");
    QStringList args;
    if (!program.isEmpty()) {
        args << "-xf" << archivePath << "-C" << destinationDir;
    } else {
        program = QStandardPaths::findExecutable("unzip");
        args << "-q" << archivePath << "-d" << destinationDir;
    }

    if (program.isEmpty()) {
        *error = "Neither bsdtar nor unzip was found on PATH.";
        return false;
    }

    QProcess process;
    process.start(program, args);
    if (!process.waitForFinished(-1) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = "Archive extraction failed: " + QString::fromUtf8(process.readAllStandardError()).trimmed();
        return false;
    }
    return true;
}

QStringList copyMappedFiles(const QString &quakeDir, const QString &sourceRoot, const QString &sourcePrefix, const QString &destPrefix, bool *ok)
{
    *ok = false;
    QString sourceDirPath;
    if (!safeChildPath(sourceRoot, sourcePrefix, &sourceDirPath) || !QDir(sourceDirPath).exists()) {
        return {};
    }

    QString destDirPath;
    if (!safeChildPath(quakeDir, destPrefix, &destDirPath)) {
        return {};
    }

    QStringList copiedFiles;
    QDirIterator it(sourceDirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString sourceFile = it.next();
        const QString rel = QDir(sourceDirPath).relativeFilePath(sourceFile);
        const QString destFile = QDir(destDirPath).filePath(rel);
        QDir().mkpath(QFileInfo(destFile).path());
        QFile::remove(destFile);
        if (!QFile::copy(sourceFile, destFile)) {
            return copiedFiles;
        }
        copiedFiles << QDir::cleanPath(destFile);
    }
    *ok = true;
    return copiedFiles;
}

QJsonObject extractMappingObject(const ModEntry &entry)
{
    const QJsonValue mapping = entry.install.value("extractmapping");
    return mapping.isObject() ? mapping.toObject() : QJsonObject();
}

QString extractDestination(const ModEntry &entry)
{
    QString extract = entry.install.value("extract").toString();
    extract.replace("{base}", "");
    return extract;
}
