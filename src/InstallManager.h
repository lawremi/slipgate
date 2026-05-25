#pragma once

#include "ModEntry.h"

#include <QHash>
#include <QJsonObject>
#include <QSet>

struct InstallState {
    QSet<QString> installedSha;
    QHash<QString, QString> installedAt;
    QHash<QString, QStringList> installedFiles;
};

QString appDataDir();
QString downloadsDir();
QString manifestPathFor(const ModEntry &entry);
QString archivePathFor(const ModEntry &entry);
bool verifySha256(const QString &path, const QString &expected);
InstallState loadInstallManifests();
void saveInstallManifest(const ModEntry &entry, const QStringList &files, InstallState *state);
bool extractZip(const QString &archivePath, const QString &destinationDir, QString *error);
QStringList copyMappedFiles(const QString &quakeDir, const QString &sourceRoot, const QString &sourcePrefix, const QString &destPrefix, bool *ok);
QJsonObject extractMappingObject(const ModEntry &entry);
QString extractDestination(const ModEntry &entry);
