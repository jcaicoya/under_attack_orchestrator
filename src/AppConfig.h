#pragma once
#include <QString>
#include <QStringList>
#include <QList>

struct AppEntry {
    QString    id;
    QString    name;
    QString    description;
    QString    executable;
    QString    workingDirectory;
    QStringList arguments;
    QString    startupPolicy       = "manual";
    QString    closePolicy         = "terminateThenKill";
    QString    expectedWindowTitle;
    QString    category;
    QString    linkedAndroidId;    // optional — references an AndroidEntry id
};

class AppConfig {
public:
    bool loadFromFile(const QString& path);
    bool saveToFile(const QString& path) const;

    const QList<AppEntry>& apps() const { return m_apps; }
    void addApp(const AppEntry& e)               { m_apps.append(e); }
    void updateApp(int index, const AppEntry& e) { m_apps[index] = e; }
    void removeApp(int index)                    { m_apps.removeAt(index); }

    static bool copyDefaultTo(const QString& destPath);

private:
    QList<AppEntry> m_apps;
    int             m_version = 1;
};
