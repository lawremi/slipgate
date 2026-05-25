#include "Launcher.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

QStringList gameDirectoriesFor(const ModEntry &entry, const QString &quakeDir)
{
    QStringList dirs;
    const QStringList args = QProcess::splitCommand(entry.commandLine);
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg == "-game" && i + 1 < args.size()) {
            dirs << QDir(quakeDir).filePath(args.at(++i));
            continue;
        }
        if (arg == "-hipnotic") {
            dirs << QDir(quakeDir).filePath("hipnotic");
            continue;
        }
        if (arg == "-rogue") {
            dirs << QDir(quakeDir).filePath("rogue");
            continue;
        }
    }

    if (dirs.isEmpty()) {
        dirs << QDir(quakeDir).filePath("id1");
    }
    dirs.removeDuplicates();
    return dirs;
}

QString newestSaveNameIn(const QString &gameDir)
{
    QFileInfo newest;
    QDirIterator it(gameDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo save(it.next());
        if (save.suffix().compare("sav", Qt::CaseInsensitive) != 0) {
            continue;
        }
        if (!newest.exists() || save.lastModified() > newest.lastModified()) {
            newest = save;
        }
    }

    if (!newest.exists()) {
        return {};
    }

    QString relative = QDir(gameDir).relativeFilePath(newest.filePath());
    if (relative.endsWith(".sav", Qt::CaseInsensitive)) {
        relative.chop(4);
    }
    return QDir::fromNativeSeparators(relative);
}

QString newestSaveName(const ModEntry &entry, const QString &quakeDir)
{
    QFileInfo newestFile;
    QString newestName;

    for (const QString &gameDir : gameDirectoriesFor(entry, quakeDir)) {
        const QString saveName = newestSaveNameIn(gameDir);
        if (saveName.isEmpty()) {
            continue;
        }

        const QFileInfo saveFile(QDir(gameDir).filePath(saveName + ".sav"));
        if (!newestFile.exists() || saveFile.lastModified() > newestFile.lastModified()) {
            newestFile = saveFile;
            newestName = saveName;
        }
    }

    return newestName;
}

} // namespace

QStringList launchArgumentsFor(const ModEntry &entry, const QString &quakeDir, const LaunchOptions &options, QString *error)
{
    QStringList args;
    if (!entry.commandLine.isEmpty()) {
        args << QProcess::splitCommand(entry.commandLine);
    }

    if (options.loadLatestSave) {
        const QString saveName = newestSaveName(entry, quakeDir);
        if (saveName.isEmpty()) {
            if (error) {
                *error = "No save files were found for " + entry.displayTitle() + ".";
            }
            return {};
        }
        args << "+load" << saveName;
    } else if (!entry.primaryStartMap().isEmpty()) {
        args << "+map" << entry.primaryStartMap();
    }
    return args;
}

bool launchMod(const QString &clientExecutable, const QString &quakeDir, const ModEntry &entry, const LaunchOptions &options, QString *error)
{
    const QStringList args = launchArgumentsFor(entry, quakeDir, options, error);
    if (options.loadLatestSave && args.isEmpty()) {
        return false;
    }

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
