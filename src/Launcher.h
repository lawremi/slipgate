#pragma once

#include "ModEntry.h"

#include <QString>

QStringList launchArgumentsFor(const ModEntry &entry);
bool launchMod(const QString &clientExecutable, const QString &quakeDir, const ModEntry &entry, QString *error);
