#pragma once

#include "ModEntry.h"

#include <QSet>

QList<ModEntry> filterAndSortEntries(const QList<ModEntry> &catalog, const QString &query, const QSet<QString> &installedSha);
bool entryMatchesSearchQuery(const ModEntry &entry, const QString &query);
