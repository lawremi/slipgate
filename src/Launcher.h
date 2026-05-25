#pragma once

#include "ModEntry.h"

#include <QString>

struct LaunchOptions {
    bool loadLatestSave = false;
};

QStringList launchArgumentsFor(const QString &clientExecutable, const ModEntry &entry, const QString &quakeDir, const LaunchOptions &options, QString *error);
bool launchMod(const QString &clientExecutable, const QString &quakeDir, const ModEntry &entry, const LaunchOptions &options, QString *error);
