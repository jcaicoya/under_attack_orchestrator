#pragma once
#include <QString>
#include <QList>

struct MediaEntry {
    QString id;
    QString name;
    QString type;     // "video" or "audio"
    QString path;
    bool    enabled = true;
};

class MediaConfig {
public:
    bool loadFromFile(const QString& path);
    bool saveToFile(const QString& path) const;

    const QList<MediaEntry>& items() const { return m_items; }
    void addItem(const MediaEntry& e)                { m_items.append(e); }
    void updateItem(int index, const MediaEntry& e)  { m_items[index] = e; }
    void removeItem(int index)                       { m_items.removeAt(index); }

private:
    QList<MediaEntry> m_items;
    int               m_version = 1;
};
