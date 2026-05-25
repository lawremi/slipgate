#include "CatalogService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedPointer>
#include <QUrlQuery>

namespace {

constexpr auto QuaddictedApi = "https://www.quaddicted.com/api/v1/";

} // namespace

CatalogService::CatalogService(QObject *parent)
    : QObject(parent)
{
}

void CatalogService::refresh()
{
    emit refreshStarted();

    QUrl url(QuaddictedApi);
    QUrlQuery query;
    query.addQueryItem("q", catalogQuery());
    query.addQueryItem("rows", "10000");
    query.addQueryItem("fl", "sha256,tags,urls,bytes,description,install");
    url.setQuery(query);

    auto *reply = network_.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> guard(reply);
        if (reply->error() != QNetworkReply::NoError) {
            emit refreshFailed("Quaddicted request failed: " + reply->errorString());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            emit refreshFailed("Quaddicted returned an unexpected response.");
            return;
        }

        QList<ModEntry> entries;
        for (const QJsonValue &value : doc.array()) {
            if (value.isObject()) {
                entries << modEntryFromJson(value.toObject());
            }
        }
        emit refreshFinished(entries);
    });
}

QString CatalogService::catalogQuery() const
{
    return "+tags:\"game=quake\" +tags:\"game_mode=singleplayer\"";
}
