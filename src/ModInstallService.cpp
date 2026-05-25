#include "ModInstallService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedPointer>
#include <QTemporaryDir>
#include <QUrlQuery>

namespace {

constexpr auto QuaddictedApi = "https://www.quaddicted.com/api/v1/";

} // namespace

ModInstallService::ModInstallService(QObject *parent)
    : QObject(parent)
    , state_(loadInstallManifests())
{
}

const InstallState &ModInstallService::state() const
{
    return state_;
}

bool ModInstallService::isBusy() const
{
    return busy_;
}

bool ModInstallService::isInstalled(const QString &sha256) const
{
    return state_.installedSha.contains(sha256);
}

bool ModInstallService::isArchiveCached(const ModEntry &entry) const
{
    return archiveExists(entry) && archiveChecksumValid(entry);
}

bool ModInstallService::archiveExists(const ModEntry &entry) const
{
    return QFileInfo::exists(archivePathFor(entry));
}

bool ModInstallService::archiveChecksumValid(const ModEntry &entry) const
{
    return verifySha256(archivePathFor(entry), entry.sha256);
}

QString ModInstallService::installingSha() const
{
    return installingSha_;
}

QString ModInstallService::downloadsPath() const
{
    return downloadsDir();
}

void ModInstallService::installWithDependencies(const ModEntry &entry, const QList<ModEntry> &catalog, const QString &quakeDir)
{
    if (busy_) {
        emit statusChanged("An install is already in progress.");
        return;
    }
    if (entry.sha256.isEmpty()) {
        emit statusChanged("Choose a mod first.");
        return;
    }
    const QString cleanQuakeDir = quakeDir.trimmed();
    if (cleanQuakeDir.isEmpty() || !QDir(cleanQuakeDir).exists()) {
        emit statusChanged("Choose your Quake folder before installing.");
        return;
    }

    catalog_ = catalog;
    quakeDir_ = cleanQuakeDir;
    installQueue_.clear();
    dependencyQueue_.clear();
    queuedSha_.clear();
    queuedDependencyNames_.clear();

    enqueueEntry(entry);
    installingSha_ = entry.sha256;

    setBusy(true, "Resolving dependencies...");
    emit stateChanged();
    resolveNextDependency();
}

void ModInstallService::uninstall(const ModEntry &entry, const QString &quakeDir)
{
    if (entry.sha256.isEmpty() || !isInstalled(entry.sha256)) {
        return;
    }

    const QString quakeRoot = QFileInfo(quakeDir.trimmed()).canonicalFilePath();
    QStringList files = state_.installedFiles.value(entry.sha256);
    std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
        return a.size() > b.size();
    });

    int removed = 0;
    for (const QString &file : files) {
        const QString clean = QDir::cleanPath(file);
        if (quakeRoot.isEmpty() || (clean != quakeRoot && !clean.startsWith(quakeRoot + '/'))) {
            continue;
        }
        if (QFile::exists(clean) && QFile::remove(clean)) {
            ++removed;
        }

        QDir dir(QFileInfo(clean).path());
        while (dir.path().startsWith(quakeRoot + '/') && dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            const QString path = dir.path();
            dir.cdUp();
            QDir().rmdir(path);
        }
    }

    QFile::remove(manifestPathFor(entry));
    state_.installedSha.remove(entry.sha256);
    state_.installedAt.remove(entry.sha256);
    state_.installedFiles.remove(entry.sha256);
    emit stateChanged();
    emit statusChanged(QString("Uninstalled %1 files for %2.").arg(removed).arg(entry.displayTitle()));
}

void ModInstallService::deleteArchive(const ModEntry &entry)
{
    if (entry.sha256.isEmpty()) {
        return;
    }
    const QString archivePath = archivePathFor(entry);
    if (!QFileInfo::exists(archivePath)) {
        emit stateChanged();
        return;
    }

    if (QFile::remove(archivePath)) {
        emit statusChanged("Deleted cached archive for " + entry.displayTitle() + ".");
    } else {
        emit statusChanged("Could not delete cached archive: " + archivePath);
    }
    emit stateChanged();
}

void ModInstallService::ensureDownloadsDir()
{
    QDir().mkpath(downloadsDir());
}

void ModInstallService::setBusy(bool busy, const QString &message)
{
    busy_ = busy;
    emit busyChanged(busy_, message);
    if (!message.isEmpty()) {
        emit statusChanged(message);
    }
}

void ModInstallService::fail(const QString &message)
{
    installingSha_.clear();
    setBusy(false);
    emit statusChanged(message);
    emit stateChanged();
}

void ModInstallService::finish(const QString &message)
{
    installingSha_.clear();
    setBusy(false);
    emit statusChanged(message);
    emit stateChanged();
}

void ModInstallService::enqueueEntry(const ModEntry &entry)
{
    if (entry.sha256.isEmpty() || queuedSha_.contains(entry.sha256)) {
        return;
    }
    installQueue_ << entry;
    queuedSha_.insert(entry.sha256);

    for (const QString &dependency : entry.dependencies) {
        if (!queuedDependencyNames_.contains(dependency)) {
            dependencyQueue_.enqueue(dependency);
            queuedDependencyNames_.insert(dependency);
        }
    }
}

void ModInstallService::resolveNextDependency()
{
    if (dependencyQueue_.isEmpty()) {
        std::reverse(installQueue_.begin(), installQueue_.end());
        installNextQueuedMod();
        return;
    }

    const QString dependency = dependencyQueue_.dequeue();
    const ModEntry localDependency = catalogDependency(dependency);
    if (!localDependency.sha256.isEmpty()) {
        enqueueEntry(localDependency);
        resolveNextDependency();
        return;
    }

    QUrl url(QuaddictedApi);
    QUrlQuery query;
    query.addQueryItem("q", dependencyQuery(dependency));
    query.addQueryItem("rows", "1");
    query.addQueryItem("fl", "sha256,tags,urls,bytes,description,install");
    url.setQuery(query);

    emit statusChanged("Resolving dependency " + dependency + "...");
    auto *reply = network_.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, dependency] {
        const QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);
        if (reply->error() != QNetworkReply::NoError) {
            fail("Dependency lookup failed for " + dependency + ": " + reply->errorString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray() || doc.array().isEmpty()) {
            fail("Could not find dependency in Quaddicted: " + dependency);
            return;
        }

        enqueueEntry(modEntryFromJson(doc.array().first().toObject()));
        resolveNextDependency();
    });
}

QString ModInstallService::dependencyQuery(const QString &dependency) const
{
    return QString("+tags:\"filename=%1.zip\"").arg(dependency);
}

ModEntry ModInstallService::catalogDependency(const QString &dependency) const
{
    const QString filename = dependency.endsWith(".zip", Qt::CaseInsensitive) ? dependency : dependency + ".zip";
    for (const ModEntry &entry : catalog_) {
        if (entry.filename.compare(filename, Qt::CaseInsensitive) == 0) {
            return entry;
        }
    }
    return {};
}

void ModInstallService::installNextQueuedMod()
{
    if (installQueue_.isEmpty()) {
        finish("Install complete.");
        return;
    }

    downloadThenInstall(installQueue_.takeFirst());
}

void ModInstallService::downloadThenInstall(const ModEntry &entry)
{
    QDir().mkpath(downloadsDir());
    const QString archivePath = archivePathFor(entry);
    if (QFileInfo::exists(archivePath) && verifySha256(archivePath, entry.sha256)) {
        installArchive(entry, archivePath);
        return;
    }

    if (entry.urls.isEmpty()) {
        fail("No download URL for " + entry.displayTitle());
        return;
    }

    emit statusChanged("Downloading " + entry.displayTitle() + "...");
    auto *reply = network_.get(QNetworkRequest(QUrl(entry.urls.front())));
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit progressChanged(received, total);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, entry, archivePath] {
        const QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);
        emit progressChanged(0, 0);
        if (reply->error() != QNetworkReply::NoError) {
            fail("Download failed for " + entry.displayTitle() + ": " + reply->errorString());
            return;
        }

        QFile file(archivePath);
        if (!file.open(QIODevice::WriteOnly)) {
            fail("Could not write " + archivePath);
            return;
        }
        file.write(reply->readAll());
        file.close();

        if (!verifySha256(archivePath, entry.sha256)) {
            QFile::remove(archivePath);
            fail("Downloaded file failed SHA256 verification: " + entry.displayTitle());
            return;
        }
        installArchive(entry, archivePath);
    });
}

void ModInstallService::installArchive(const ModEntry &entry, const QString &archivePath)
{
    emit statusChanged("Installing " + entry.displayTitle() + "...");

    QTemporaryDir temp;
    if (!temp.isValid()) {
        fail("Could not create a temporary extraction directory.");
        return;
    }

    QString error;
    if (!extractZip(archivePath, temp.path(), &error)) {
        fail(error);
        return;
    }

    const QJsonObject mappings = extractMappingObject(entry);
    bool ok = true;
    QStringList copiedFiles;
    if (!mappings.isEmpty()) {
        for (auto it = mappings.begin(); it != mappings.end(); ++it) {
            bool mappingOk = false;
            copiedFiles << copyMappedFiles(quakeDir_, temp.path(), it.key(), it.value().toString(), &mappingOk);
            ok = mappingOk && ok;
        }
    } else {
        const QString destination = extractDestination(entry);
        QString source = "/";
        if (!entry.zipBaseDir.isEmpty()) {
            source = "/" + entry.zipBaseDir;
        }
        copiedFiles = copyMappedFiles(quakeDir_, temp.path(), source, destination.isEmpty() ? "/" : destination, &ok);
    }

    if (!ok) {
        fail("Install failed while copying files for " + entry.displayTitle());
        return;
    }

    saveInstallManifest(entry, copiedFiles, &state_);
    emit stateChanged();
    installNextQueuedMod();
}
