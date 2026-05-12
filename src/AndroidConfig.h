#pragma once
#include <QString>
#include <QList>
#include <QtTypes>

struct AndroidEntry {
    QString id;
    QString name;
    QString package;
    QString activity = ".MainActivity";
    quint16 wsPort   = 0;
};

class AndroidConfig {
public:
    bool loadFromFile(const QString& path);
    bool saveToFile(const QString& path) const;

    const QList<AndroidEntry>& apps() const { return m_apps; }
    void addApp(const AndroidEntry& e)               { m_apps.append(e); }
    void updateApp(int index, const AndroidEntry& e) { m_apps[index] = e; }
    void removeApp(int index)                        { m_apps.removeAt(index); }

    static bool copyDefaultTo(const QString& destPath);

private:
    QList<AndroidEntry> m_apps;
};
