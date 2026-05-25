#pragma once

#include "InstallManager.h"
#include "ModEntry.h"

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QSet>

class ModInstallService : public QObject {
    Q_OBJECT

public:
    explicit ModInstallService(QObject *parent = nullptr);

    [[nodiscard]] const InstallState &state() const;
    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] bool isInstalled(const QString &sha256) const;
    [[nodiscard]] bool isArchiveCached(const ModEntry &entry) const;
    [[nodiscard]] bool archiveExists(const ModEntry &entry) const;
    [[nodiscard]] bool archiveChecksumValid(const ModEntry &entry) const;
    [[nodiscard]] QString installingSha() const;
    [[nodiscard]] QString downloadsPath() const;

public slots:
    void installWithDependencies(const ModEntry &entry, const QList<ModEntry> &catalog, const QString &quakeDir);
    void uninstall(const ModEntry &entry, const QString &quakeDir);
    void deleteArchive(const ModEntry &entry);
    void ensureDownloadsDir();

signals:
    void stateChanged();
    void busyChanged(bool busy, const QString &message);
    void statusChanged(const QString &message);
    void progressChanged(qint64 received, qint64 total);

private:
    void setBusy(bool busy, const QString &message = {});
    void fail(const QString &message);
    void finish(const QString &message);
    void enqueueEntry(const ModEntry &entry);
    void resolveNextDependency();
    void installNextQueuedMod();
    void downloadThenInstall(const ModEntry &entry);
    void installArchive(const ModEntry &entry, const QString &archivePath);

    [[nodiscard]] QString dependencyQuery(const QString &dependency) const;
    [[nodiscard]] ModEntry catalogDependency(const QString &dependency) const;

    QNetworkAccessManager network_;
    InstallState state_;
    QList<ModEntry> catalog_;
    QList<ModEntry> installQueue_;
    QQueue<QString> dependencyQueue_;
    QSet<QString> queuedSha_;
    QSet<QString> queuedDependencyNames_;
    QString quakeDir_;
    QString installingSha_;
    bool busy_ = false;
};
