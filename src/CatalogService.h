#pragma once

#include "ModEntry.h"

#include <QList>
#include <QNetworkAccessManager>
#include <QObject>

class CatalogService : public QObject {
    Q_OBJECT

public:
    explicit CatalogService(QObject *parent = nullptr);

public slots:
    void refresh();

signals:
    void refreshStarted();
    void refreshFinished(const QList<ModEntry> &entries);
    void refreshFailed(const QString &message);

private:
    [[nodiscard]] QString catalogQuery() const;

    QNetworkAccessManager network_;
};
