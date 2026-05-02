#include "MediaConfig.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

bool MediaConfig::loadFromFile(const QString& path) {
    m_items.clear();
    QFile file(path);
    if (!file.exists()) return true;   // no file yet = empty list, that's fine
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return false;

    auto root = doc.object();
    m_version = root["version"].toInt(1);

    for (const auto& v : root["items"].toArray()) {
        auto o = v.toObject();
        MediaEntry e;
        e.id      = o["id"].toString();
        e.name    = o["name"].toString();
        e.type    = o["type"].toString();
        e.path    = o["path"].toString();
        e.enabled = o["enabled"].toBool(true);
        m_items.append(e);
    }
    return true;
}

bool MediaConfig::saveToFile(const QString& path) const {
    QJsonArray arr;
    for (const auto& e : m_items) {
        QJsonObject o;
        o["id"]      = e.id;
        o["name"]    = e.name;
        o["type"]    = e.type;
        o["path"]    = e.path;
        o["enabled"] = e.enabled;
        arr.append(o);
    }
    QJsonObject root;
    root["version"] = m_version;
    root["items"]   = arr;

    QDir().mkpath(QFileInfo(path).dir().absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
