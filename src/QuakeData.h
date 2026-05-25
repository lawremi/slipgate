#pragma once

#include <QString>

struct PakValidation {
    bool id1Exists = false;
    bool lowercasePak0 = false;
    bool lowercasePak1 = false;
    bool uppercasePak0 = false;
    bool uppercasePak1 = false;
};

PakValidation validatePaks(const QString &quakeDir);
bool createLowercasePakSymlinks(const QString &quakeDir);
