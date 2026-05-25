#include "ModEntry.h"

#include <QJsonArray>
#include <QLocale>
#include <QRegularExpression>

namespace {

QString removeOuterQuotes(QString value)
{
    value = value.trimmed();
    if ((value.startsWith('\'') && value.endsWith('\'')) ||
        (value.startsWith('"') && value.endsWith('"'))) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

QStringList linkValues(const QStringList &tags)
{
    QStringList values;
    const QRegularExpression markdownLink(R"(^\[([^\]]+)\]\(([^)]+)\)$)");

    for (const QString &tag : tags) {
        QString label;
        QString url;
        if (tag.startsWith("link=")) {
            const QString raw = tag.mid(5).trimmed();
            const auto match = markdownLink.match(raw);
            if (match.hasMatch()) {
                label = match.captured(1);
                url = match.captured(2);
            } else {
                label = "Link";
                url = raw;
            }
        } else if (tag.startsWith("link:")) {
            const qsizetype equals = tag.indexOf('=');
            if (equals > 5) {
                label = tag.mid(5, equals - 5);
                url = tag.mid(equals + 1);
            }
        }

        if (!url.isEmpty()) {
            values << removeOuterQuotes(label.isEmpty() ? QString("Link") : label) + "\t" + removeOuterQuotes(url);
        }
    }

    values.removeDuplicates();
    return values;
}

} // namespace

QString tagValue(const QStringList &tags, const QString &prefix)
{
    const QString needle = prefix + '=';
    for (const QString &tag : tags) {
        if (tag.startsWith(needle)) {
            return removeOuterQuotes(tag.mid(needle.size()));
        }
    }
    return {};
}

QStringList tagValues(const QStringList &tags, const QString &prefix)
{
    QStringList values;
    const QString needle = prefix + '=';
    for (const QString &tag : tags) {
        if (tag.startsWith(needle)) {
            values.push_back(removeOuterQuotes(tag.mid(needle.size())));
        }
    }
    values.removeDuplicates();
    return values;
}

QString ModEntry::displayTitle() const
{
    if (!title.isEmpty()) {
        return title;
    }
    if (!filename.isEmpty()) {
        return filename;
    }
    return sha256.left(12);
}

QString ModEntry::primaryStartMap() const
{
    if (!startMaps.isEmpty()) {
        return startMaps.front();
    }
    return {};
}

QString ModEntry::summaryLine() const
{
    QStringList parts;
    if (!authors.isEmpty()) {
        parts << authors.join(", ");
    }
    if (!releaseDate.isEmpty()) {
        parts << releaseDate;
    }
    if (bytes > 0) {
        parts << QLocale().formattedDataSize(bytes);
    }
    return parts.join("  |  ");
}

QString ModEntry::descriptionText() const
{
    QString text = descriptionHtml;
    text.replace(QRegularExpression("<[^>]*>"), "");
    text.replace("&quot;", "\"");
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    return text.trimmed();
}

ModEntry modEntryFromJson(const QJsonObject &object)
{
    ModEntry entry;
    entry.sha256 = object.value("sha256").toString();
    entry.descriptionHtml = object.value("description").toString();
    entry.bytes = static_cast<qint64>(object.value("bytes").toDouble());
    entry.install = object.value("install").toObject();

    const auto tagsArray = object.value("tags").toArray();
    QStringList tags;
    tags.reserve(tagsArray.size());
    for (const QJsonValue &value : tagsArray) {
        tags << value.toString();
    }
    entry.tags = tags;

    const auto urlsArray = object.value("urls").toArray();
    entry.urls.reserve(urlsArray.size());
    for (const QJsonValue &value : urlsArray) {
        entry.urls << value.toString();
    }

    entry.title = tagValue(tags, "title");
    entry.filename = tagValue(tags, "filename");
    entry.commandLine = tagValue(tags, "commandline");
    entry.releaseDate = tagValue(tags, "release_date");
    entry.zipBaseDir = tagValue(tags, "zipbasedir");
    entry.authors = tagValues(tags, "author");
    entry.startMaps = tagValues(tags, "startmap");
    entry.dependencies = tagValues(tags, "dependency");
    entry.links = linkValues(tags);

    return entry;
}
