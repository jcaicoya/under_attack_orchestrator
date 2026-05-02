#pragma once
#include <QString>
#include <QStringList>
#include <QList>

struct AppEntry {
    QString    id;
    QString    name;
    QString    description;
    bool       enabled              = true;
    QString    executable;
    QString    workingDirectory;
    QStringList arguments;
    QStringList configurationArguments;
    QStringList rehearsalArguments;
    QStringList liveArguments;
    QString    startupPolicy       = "manual";
    QString    closePolicy         = "terminateThenKill";
    QString    expectedWindowTitle;
    QString    category;
};

class AppConfig {
public:
    bool loadFromFile(const QString& path);
    const QList<AppEntry>& apps() const { return m_apps; }
    static bool copyDefaultTo(const QString& destPath);

private:
    QList<AppEntry> m_apps;
    int             m_version = 1;
};
