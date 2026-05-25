#include "Launcher.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

QStringList gameDirectoryNamesFor(const ModEntry &entry)
{
    QStringList names;
    const QStringList args = QProcess::splitCommand(entry.commandLine);
    for (qsizetype i = 0; i < args.size(); ++i) {
        const QString arg = args.at(i);
        if (arg == "-game" && i + 1 < args.size()) {
            names << args.at(++i);
            continue;
        }
        if (arg == "-hipnotic") {
            names << "hipnotic";
            continue;
        }
        if (arg == "-rogue") {
            names << "rogue";
            continue;
        }
    }

    if (names.isEmpty()) {
        names << "id1";
    }
    names.removeDuplicates();
    return names;
}

QStringList sourcePortSaveRoots(const QString &clientExecutable)
{
    const QString clientName = QFileInfo(clientExecutable).baseName().toLower();
    QStringList roots;

#ifdef Q_OS_WIN
    // Slipgate launches source ports with -basedir, and the Windows builds of
    // the QuakeSpasm family generally use the active game dir under that base.
    // Do not guess at unrelated per-port folders here; a found save must also
    // be loadable by "+load <name>" in the launched process.
    Q_UNUSED(clientName);
#elif defined(Q_OS_MACOS)
    if (clientName.contains("ironwail")) {
        roots << QDir::homePath() + "/Library/Application Support/Ironwail";
    }
    if (clientName.contains("vkquake")) {
        roots << QDir::homePath() + "/Library/Application Support/vkQuake";
    }
    if (clientName.contains("quakespasm") || clientName == "qss") {
        roots << QDir::homePath() + "/Library/Application Support/QuakeSpasm";
    }
#else
    if (clientName.contains("ironwail")) {
        roots << QDir::homePath() + "/.ironwail";
    } else if (clientName.contains("vkquake")) {
        roots << QDir::homePath() + "/.vkquake";
    } else if (clientName.contains("quakespasm") || clientName == "qss") {
        roots << QDir::homePath() + "/.quakespasm";
    }
#endif

    roots.removeDuplicates();
    return roots;
}

QStringList saveDirectoriesFor(const QString &clientExecutable, const ModEntry &entry, const QString &quakeDir)
{
    QStringList dirs;
    const QStringList names = gameDirectoryNamesFor(entry);
    for (const QString &name : names) {
        dirs << QDir(quakeDir).filePath(name);
        for (const QString &root : sourcePortSaveRoots(clientExecutable)) {
            dirs << QDir(root).filePath(name);
        }
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

QString newestSaveName(const QString &clientExecutable, const ModEntry &entry, const QString &quakeDir, QStringList *searchedDirs)
{
    QFileInfo newestFile;
    QString newestName;

    for (const QString &gameDir : saveDirectoriesFor(clientExecutable, entry, quakeDir)) {
        if (searchedDirs) {
            *searchedDirs << gameDir;
        }
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

QStringList launchArgumentsFor(const QString &clientExecutable, const ModEntry &entry, const QString &quakeDir, const LaunchOptions &options, QString *error)
{
    QStringList args;
    if (!entry.commandLine.isEmpty()) {
        args << QProcess::splitCommand(entry.commandLine);
    }

    if (options.loadLatestSave) {
        QStringList searchedDirs;
        const QString saveName = newestSaveName(clientExecutable, entry, quakeDir, &searchedDirs);
        if (saveName.isEmpty()) {
            if (error) {
                *error = "No save files were found for " + entry.displayTitle() + ". Searched: " + searchedDirs.join(", ");
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
    const QStringList args = launchArgumentsFor(clientExecutable, entry, quakeDir, options, error);
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
