#include "AndroidConfig.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

bool AndroidConfig::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError)
        return false;

    m_apps.clear();
    for (const auto& v : doc.object()["apps"].toArray()) {
        auto o = v.toObject();
        AndroidEntry e;
        e.id       = o["id"].toString();
        e.name     = o["name"].toString();
        e.package  = o["package"].toString();
        e.activity = o["activity"].toString(".MainActivity");
        e.wsPort   = static_cast<quint16>(o["wsPort"].toInt(0));
        m_apps.append(e);
    }
    return true;
}

bool AndroidConfig::saveToFile(const QString& path) const {
    QJsonArray arr;
    for (const auto& e : m_apps) {
        QJsonObject o;
        o["id"]       = e.id;
        o["name"]     = e.name;
        o["package"]  = e.package;
        o["activity"] = e.activity;
        o["wsPort"]   = e.wsPort;
        arr.append(o);
    }

    QJsonObject root;
    root["apps"] = arr;

    QDir().mkpath(QFileInfo(path).dir().absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool AndroidConfig::copyDefaultTo(const QString& destPath) {
    QFile src(":/defaults/resources/android_default.json");
    if (!src.open(QIODevice::ReadOnly))
        return false;

    QDir().mkpath(QFileInfo(destPath).dir().absolutePath());
    QFile dst(destPath);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    dst.write(src.readAll());
    return true;
}
