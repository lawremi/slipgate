#include "SearchFilter.h"

#include <QProcess>

namespace {

struct SearchToken {
    QString field;
    QString value;
    bool negated = false;
};

QStringList valuesForField(const ModEntry &entry, QString field)
{
    field = field.toLower();
    if (field == "title") {
        return {entry.displayTitle()};
    }
    if (field == "author") {
        return entry.authors;
    }
    if (field == "filename" || field == "archive") {
        return {entry.filename};
    }
    if (field == "commandline" || field == "command" || field == "cmd") {
        return {entry.commandLine};
    }
    if (field == "release_date" || field == "date" || field == "released") {
        return {entry.releaseDate};
    }
    if (field == "startmap" || field == "map") {
        return entry.startMaps;
    }
    if (field == "dependency" || field == "depends") {
        return entry.dependencies;
    }
    if (field == "sha" || field == "sha256") {
        return {entry.sha256};
    }
    if (field == "description" || field == "desc") {
        return {entry.descriptionText()};
    }
    if (field == "tag") {
        return entry.tags;
    }

    return tagValues(entry.tags, field);
}

QList<SearchToken> parseSearch(const QString &query)
{
    QList<SearchToken> tokens;
    for (QString part : QProcess::splitCommand(query)) {
        part = part.trimmed();
        if (part.isEmpty()) {
            continue;
        }

        SearchToken token;
        if (part.startsWith('-') && part.size() > 1) {
            token.negated = true;
            part.remove(0, 1);
        }

        const qsizetype colon = part.indexOf(':');
        if (colon > 0) {
            token.field = part.left(colon).toLower();
            token.value = part.mid(colon + 1);
        } else {
            token.field = "title";
            token.value = part;
        }

        if (!token.value.isEmpty()) {
            tokens << token;
        }
    }
    return tokens;
}

bool entryMatchesToken(const ModEntry &entry, const SearchToken &token)
{
    bool matched = false;
    for (const QString &value : valuesForField(entry, token.field)) {
        if (value.contains(token.value, Qt::CaseInsensitive)) {
            matched = true;
            break;
        }
    }
    return token.negated ? !matched : matched;
}

bool entryMatchesTokens(const ModEntry &entry, const QList<SearchToken> &tokens)
{
    for (const SearchToken &token : tokens) {
        if (!entryMatchesToken(entry, token)) {
            return false;
        }
    }
    return true;
}

} // namespace

QList<ModEntry> filterAndSortEntries(const QList<ModEntry> &catalog, const QString &query, const QSet<QString> &installedSha)
{
    const QList<SearchToken> tokens = parseSearch(query);
    QList<ModEntry> entries;
    entries.reserve(catalog.size());
    for (const ModEntry &entry : catalog) {
        if (entryMatchesTokens(entry, tokens)) {
            entries << entry;
        }
    }
    std::stable_sort(entries.begin(), entries.end(), [&installedSha](const ModEntry &a, const ModEntry &b) {
        return installedSha.contains(a.sha256) && !installedSha.contains(b.sha256);
    });
    return entries;
}

bool entryMatchesSearchQuery(const ModEntry &entry, const QString &query)
{
    return entryMatchesTokens(entry, parseSearch(query));
}
