#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

struct ModEntry {
    QString sha256;
    QString title;
    QString filename;
    QString descriptionHtml;
    QString commandLine;
    QString releaseDate;
    QStringList authors;
    QStringList startMaps;
    QStringList dependencies;
    QStringList links;
    QStringList tags;
    QStringList urls;
    QString zipBaseDir;
    qint64 bytes = 0;
    QJsonObject install;

    [[nodiscard]] QString displayTitle() const;
    [[nodiscard]] QString primaryStartMap() const;
    [[nodiscard]] QString summaryLine() const;
    [[nodiscard]] QString descriptionText() const;
};

ModEntry modEntryFromJson(const QJsonObject &object);
QString tagValue(const QStringList &tags, const QString &prefix);
QStringList tagValues(const QStringList &tags, const QString &prefix);
