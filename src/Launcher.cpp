#include "Launcher.h"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

QStringList launchArgumentsFor(const ModEntry &entry)
{
    QStringList args;
    if (!entry.commandLine.isEmpty()) {
        args << QProcess::splitCommand(entry.commandLine);
    }
    if (!entry.primaryStartMap().isEmpty()) {
        args << "+map" << entry.primaryStartMap();
    }
    return args;
}

bool launchMod(const QString &clientExecutable, const QString &quakeDir, const ModEntry &entry, QString *error)
{
    const QStringList args = launchArgumentsFor(entry);

    if (!clientExecutable.isEmpty() && QFileInfo(clientExecutable).isExecutable()) {
        QStringList clientArgs;
        if (!quakeDir.isEmpty()) {
            clientArgs << "-basedir" << quakeDir;
        }
        clientArgs << args;
        if (!QProcess::startDetached(clientExecutable, clientArgs, quakeDir)) {
            *error = "Could not launch " + clientExecutable;
            return false;
        }
        return true;
    }

    const QString steam = QStandardPaths::findExecutable("steam");
    if (!steam.isEmpty()) {
        QStringList steamArgs{"-applaunch", "2310"};
        steamArgs << args;
        if (!QProcess::startDetached(steam, steamArgs)) {
            *error = "Could not launch Quake through Steam.";
            return false;
        }
        return true;
    }

    *error = "Choose a source port executable or install Steam.";
    return false;
}
