#pragma once

#include <QString>
#include <QStringList>

struct DetectedEnvironment {
    QString quakeDir;
    QString clientExecutable;
    QString clientName;
    QString steamExecutable;
    QStringList notes;
};

class EnvironmentDetector {
public:
    static DetectedEnvironment detect();

private:
    static QStringList steamRoots();
    static QStringList steamLibrariesContainingApp(const QString &steamRoot, const QString &appId);
    static QString quakeDirFromLibrary(const QString &libraryRoot);
    static QString findClient(const QString &quakeDir, QString *clientName);
    static QString findSteamExecutable(const QStringList &steamRoots);
    static QString findOnPath(const QString &program);
};
