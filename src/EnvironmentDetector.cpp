#include "EnvironmentDetector.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <QSettings>
#endif
#include <QStandardPaths>

namespace {

constexpr auto QuakeSteamAppId = "2310";

struct VdfPair {
    QStringList path;
    QString key;
    QString value;
};

struct ClientCandidate {
    QString name;
    QStringList executableNames;
    QStringList desktopIds;
    QStringList macBundleNames;
};

QString homePath()
{
    return QDir::homePath();
}

QString cleanedPath(QString path)
{
    path.replace("\\\\", "\\");
    return QDir::cleanPath(path);
}

QString canonicalExecutable(const QString &path)
{
    const QFileInfo info(path);
    if (info.exists() && info.isFile() && info.isExecutable()) {
        return info.canonicalFilePath();
    }
    return {};
}

QString readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool steamRootIsValid(const QString &path)
{
    return QFileInfo::exists(cleanedPath(path) + "/config/libraryfolders.vdf");
}

void skipVdfWhitespaceAndComments(const QString &text, qsizetype *pos)
{
    while (*pos < text.size()) {
        if (text.at(*pos).isSpace()) {
            ++*pos;
            continue;
        }
        if (text.mid(*pos, 2) == "//") {
            while (*pos < text.size() && text.at(*pos) != '\n') {
                ++*pos;
            }
            continue;
        }
        break;
    }
}

QString parseVdfString(const QString &text, qsizetype *pos)
{
    skipVdfWhitespaceAndComments(text, pos);
    if (*pos >= text.size() || text.at(*pos) != '"') {
        return {};
    }

    QString out;
    ++*pos;
    while (*pos < text.size()) {
        const QChar ch = text.at(*pos);
        ++*pos;
        if (ch == '"') {
            break;
        }
        if (ch == '\\' && *pos < text.size()) {
            const QChar escaped = text.at(*pos);
            ++*pos;
            switch (escaped.toLatin1()) {
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            default:
                out += escaped;
                break;
            }
            continue;
        }
        out += ch;
    }
    return out;
}

QList<VdfPair> parseVdfPairs(const QString &text)
{
    QList<VdfPair> pairs;
    QStringList stack;
    qsizetype pos = 0;

    while (pos < text.size()) {
        skipVdfWhitespaceAndComments(text, &pos);
        if (pos >= text.size()) {
            break;
        }
        if (text.at(pos) == '}') {
            if (!stack.isEmpty()) {
                stack.removeLast();
            }
            ++pos;
            continue;
        }
        if (text.at(pos) != '"') {
            ++pos;
            continue;
        }

        const QString key = parseVdfString(text, &pos);
        skipVdfWhitespaceAndComments(text, &pos);
        if (pos < text.size() && text.at(pos) == '{') {
            stack << key;
            ++pos;
            continue;
        }
        if (pos < text.size() && text.at(pos) == '"') {
            pairs << VdfPair{stack, key, parseVdfString(text, &pos)};
        }
    }

    return pairs;
}

QString vdfValue(const QList<VdfPair> &pairs, const QStringList &path, const QString &key)
{
    for (const VdfPair &pair : pairs) {
        if (pair.path == path && pair.key == key) {
            return pair.value;
        }
    }
    return {};
}

QString desktopExecCommand(const QString &desktopFile)
{
    const QString text = readFile(desktopFile);
    bool inDesktopEntry = false;

    for (const QString &rawLine : text.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        if (line.startsWith('[') && line.endsWith(']')) {
            inDesktopEntry = (line == "[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry || !line.startsWith("Exec=")) {
            continue;
        }

        QString exec = line.mid(5);
        exec.remove(QRegularExpression(R"(\s%[fFuUdDnNickvm])"));
        exec.remove(QRegularExpression(R"(%[fFuUdDnNickvm])"));
        const QStringList parts = QProcess::splitCommand(exec);
        return parts.isEmpty() ? QString() : parts.front();
    }

    return {};
}

QString findViaDesktopEntries(const ClientCandidate &candidate)
{
#ifndef Q_OS_LINUX
    Q_UNUSED(candidate);
    return {};
#else
    QStringList applicationDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    applicationDirs << homePath() + "/.local/share/applications";
    applicationDirs << "/usr/local/share/applications";
    applicationDirs << "/usr/share/applications";
    applicationDirs << "/var/lib/flatpak/exports/share/applications";
    applicationDirs << homePath() + "/.local/share/flatpak/exports/share/applications";
    applicationDirs.removeDuplicates();

    const QSet<QString> desktopIds(candidate.desktopIds.begin(), candidate.desktopIds.end());
    const QSet<QString> executableNames(candidate.executableNames.begin(), candidate.executableNames.end());

    for (const QString &dir : applicationDirs) {
        QDirIterator it(dir, {"*.desktop"}, QDir::Files);
        while (it.hasNext()) {
            const QFileInfo desktop(it.next());
            const QString id = desktop.fileName();
            const QString command = desktopExecCommand(desktop.filePath());
            const QString commandName = QFileInfo(command).fileName();
            if (!desktopIds.contains(id) && !executableNames.contains(commandName)) {
                continue;
            }
            if (!executableNames.contains(commandName)) {
                // Some packages expose only "flatpak run <app-id>" here. The UI stores a
                // single executable path today, so avoid returning a launcher without args.
                continue;
            }

            if (QFileInfo(command).isAbsolute()) {
                const QString path = canonicalExecutable(command);
                if (!path.isEmpty()) {
                    return path;
                }
            }

            const QString path = QStandardPaths::findExecutable(commandName);
            if (!path.isEmpty()) {
                return path;
            }
        }
    }

    return {};
#endif
}

QString findMacBundle(const ClientCandidate &candidate)
{
#ifndef Q_OS_MACOS
    Q_UNUSED(candidate);
    return {};
#else
    QStringList roots{"/Applications", homePath() + "/Applications"};
    for (const QString &bundleName : candidate.macBundleNames) {
        for (const QString &root : roots) {
            const QString bundle = root + "/" + bundleName + ".app";
            const QString executable = bundle + "/Contents/MacOS/" + bundleName;
            const QString path = canonicalExecutable(executable);
            if (!path.isEmpty()) {
                return path;
            }
        }
    }
    return {};
#endif
}

QString findWindowsAppPath(const QString &program)
{
#ifndef Q_OS_WIN
    Q_UNUSED(program);
    return {};
#else
    const QString exe = program.endsWith(".exe", Qt::CaseInsensitive) ? program : program + ".exe";
    const QStringList roots{
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exe,
        "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exe,
        "HKEY_LOCAL_MACHINE\\Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exe,
    };

    for (const QString &root : roots) {
        QSettings settings(root, QSettings::NativeFormat);
        for (const QString &key : QStringList{".", "Default"}) {
            const QString value = settings.value(key).toString();
            const QString path = canonicalExecutable(value);
            if (!path.isEmpty()) {
                return path;
            }
        }
    }
    return {};
#endif
}

QString findNearQuakeDir(const ClientCandidate &candidate, const QString &quakeDir)
{
    if (quakeDir.isEmpty() || !QDir(quakeDir).exists()) {
        return {};
    }

    QSet<QString> names;
    for (const QString &name : candidate.executableNames) {
        names.insert(name.toCaseFolded());
#ifdef Q_OS_WIN
        if (!name.endsWith(".exe", Qt::CaseInsensitive)) {
            names.insert((name + ".exe").toCaseFolded());
        }
#endif
    }

    struct PendingDir {
        QString path;
        int depth;
    };

    QList<PendingDir> queue{{quakeDir, 0}};
    while (!queue.isEmpty()) {
        const PendingDir current = queue.takeFirst();
        QDirIterator it(current.path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();
            if (info.isDir()) {
                if (current.depth < 2) {
                    queue << PendingDir{info.filePath(), current.depth + 1};
                }
                continue;
            }
            if (names.contains(info.fileName().toCaseFolded())) {
                const QString path = canonicalExecutable(info.filePath());
                if (!path.isEmpty()) {
                    return path;
                }
            }
        }
    }

    return {};
}

QList<ClientCandidate> clientCandidates()
{
    return {
        {
            "Ironwail",
            {"ironwail"},
            {"ironwail.desktop", "io.github.andrei_drexler.ironwail.desktop", "org.andreidrexler.Ironwail.desktop"},
            {"Ironwail", "ironwail"},
        },
        {
            "vkQuake",
            {"vkquake"},
            {"vkquake.desktop", "io.github.vkquake.vkQuake.desktop"},
            {"vkQuake", "vkquake"},
        },
        {
            "QuakeSpasm-Spiked",
            {"quakespasm-spiked", "qss", "quakespasm-sdl2"},
            {"quakespasm-spiked.desktop", "qss.desktop"},
            {"QuakeSpasm-Spiked", "quakespasm-spiked"},
        },
        {
            "QuakeSpasm",
            {"quakespasm", "quakespasm-sdl2"},
            {"quakespasm.desktop", "io.github.quakespasm.Quakespasm.desktop"},
            {"QuakeSpasm", "quakespasm"},
        },
    };
}

} // namespace

DetectedEnvironment EnvironmentDetector::detect()
{
    DetectedEnvironment env;
    const QStringList roots = steamRoots();
    env.steamExecutable = findSteamExecutable(roots);

    for (const QString &steamRoot : roots) {
        for (const QString &library : steamLibrariesContainingApp(steamRoot, QuakeSteamAppId)) {
            const QString quake = quakeDirFromLibrary(library);
            if (!quake.isEmpty()) {
                env.quakeDir = quake;
                break;
            }
        }
        if (!env.quakeDir.isEmpty()) {
            break;
        }
    }

    if (env.quakeDir.isEmpty()) {
        env.notes << "Steam Quake install was not found automatically.";
    } else {
        env.notes << "Detected Steam Quake at " + env.quakeDir;
    }

    env.clientExecutable = findClient(env.quakeDir, &env.clientName);
    if (env.clientExecutable.isEmpty()) {
        if (!env.steamExecutable.isEmpty()) {
            env.notes << "No modern source port was found; launch will fall back to Steam app 2310.";
        } else {
            env.notes << "No source port executable or Steam command was found.";
        }
    } else {
        env.notes << "Detected " + env.clientName + " at " + env.clientExecutable;
    }

    return env;
}

QStringList EnvironmentDetector::steamRoots()
{
    QStringList roots;
    const auto processEnv = QProcessEnvironment::systemEnvironment();

#ifdef Q_OS_WIN
    const QString registryRoot = [] {
        QSettings settings("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat);
        return settings.value("SteamPath").toString();
    }();
    if (!registryRoot.isEmpty()) {
        roots << registryRoot;
    }
#else
    roots << homePath() + "/.steam/steam";
    roots << homePath() + "/.local/share/Steam";
    roots << homePath() + "/.var/app/com.valvesoftware.Steam/.steam/steam";
    roots << homePath() + "/.var/app/com.valvesoftware.Steam/.local/share/Steam";
#endif

#ifdef Q_OS_MACOS
    roots << homePath() + "/Library/Application Support/Steam";
#endif

    const QString steamDir = processEnv.value("STEAM_DIR");
    if (!steamDir.isEmpty()) {
        roots.prepend(steamDir);
    }

    for (QString &root : roots) {
        root = cleanedPath(root);
    }
    roots.removeAll({});
    roots.removeDuplicates();

    QStringList validRoots;
    for (const QString &root : roots) {
        if (steamRootIsValid(root)) {
            validRoots << root;
        }
    }
    return validRoots;
}

QStringList EnvironmentDetector::steamLibrariesContainingApp(const QString &steamRoot, const QString &appId)
{
    QStringList libraries;
    const QString vdf = readFile(steamRoot + "/config/libraryfolders.vdf");
    const QList<VdfPair> pairs = parseVdfPairs(vdf);

    QString currentLibrary;
    for (const VdfPair &pair : pairs) {
        if (pair.path.size() == 2 && pair.path[0] == "libraryfolders" && pair.key == "path") {
            currentLibrary = cleanedPath(pair.value);
            continue;
        }
        if (pair.path.size() == 3 &&
            pair.path[0] == "libraryfolders" &&
            pair.path[2] == "apps" &&
            pair.key == appId &&
            !currentLibrary.isEmpty()) {
            libraries << currentLibrary;
        }
    }

    libraries.removeDuplicates();
    return libraries;
}

QString EnvironmentDetector::quakeDirFromLibrary(const QString &libraryRoot)
{
    const QString manifestPath = cleanedPath(libraryRoot + "/steamapps/appmanifest_" + QString(QuakeSteamAppId) + ".acf");
    const QString manifestText = readFile(manifestPath);
    if (manifestText.isEmpty()) {
        return {};
    }

    const QList<VdfPair> pairs = parseVdfPairs(manifestText);
    const QString installDir = vdfValue(pairs, {"AppState"}, "installdir");
    if (installDir.isEmpty()) {
        return {};
    }

    const QString quakeDir = cleanedPath(libraryRoot + "/steamapps/common/" + installDir);
    if (QDir(quakeDir).exists()) {
        return QFileInfo(quakeDir).canonicalFilePath();
    }

    return {};
}

QString EnvironmentDetector::findClient(const QString &quakeDir, QString *clientName)
{
    for (const ClientCandidate &candidate : clientCandidates()) {
        for (const QString &program : candidate.executableNames) {
            const QString path = findOnPath(program);
            if (!path.isEmpty()) {
                *clientName = candidate.name;
                return path;
            }

            const QString appPath = findWindowsAppPath(program);
            if (!appPath.isEmpty()) {
                *clientName = candidate.name;
                return appPath;
            }
        }

        const QString desktopPath = findViaDesktopEntries(candidate);
        if (!desktopPath.isEmpty()) {
            *clientName = candidate.name;
            return desktopPath;
        }

        const QString bundlePath = findMacBundle(candidate);
        if (!bundlePath.isEmpty()) {
            *clientName = candidate.name;
            return bundlePath;
        }

        const QString nearbyPath = findNearQuakeDir(candidate, quakeDir);
        if (!nearbyPath.isEmpty()) {
            *clientName = candidate.name;
            return nearbyPath;
        }
    }

    clientName->clear();
    return {};
}

QString EnvironmentDetector::findSteamExecutable(const QStringList &steamRoots)
{
    const QString path = findOnPath("steam");
    if (!path.isEmpty()) {
        return path;
    }

    for (const QString &root : steamRoots) {
#ifdef Q_OS_WIN
        const QString candidate = canonicalExecutable(root + "/steam.exe");
#else
        const QString candidate = canonicalExecutable(root + "/steam");
#endif
        if (!candidate.isEmpty()) {
            return candidate;
        }
    }

    return {};
}

QString EnvironmentDetector::findOnPath(const QString &program)
{
    QStringList names{program};
#ifdef Q_OS_WIN
    if (!program.endsWith(".exe", Qt::CaseInsensitive)) {
        names << program + ".exe";
    }
#endif

    for (const QString &name : names) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty()) {
            return path;
        }
    }
    return {};
}
