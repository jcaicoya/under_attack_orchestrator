#include "AppConfig.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static QStringList toStringList(const QJsonArray& arr) {
    QStringList out;
    for (const auto& v : arr)
        out << v.toString();
    return out;
}

static QJsonArray toJsonArray(const QStringList& list) {
    QJsonArray arr;
    for (const auto& s : list) arr.append(s);
    return arr;
}

bool AppConfig::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError)
        return false;

    auto root = doc.object();
    m_version = root["version"].toInt(1);
    m_apps.clear();

    for (const auto& v : root["apps"].toArray()) {
        auto o = v.toObject();
        AppEntry e;
        e.id                     = o["id"].toString();
        e.name                   = o["name"].toString();
        e.description            = o["description"].toString();
        e.executable             = o["executable"].toString();
        e.workingDirectory       = o["workingDirectory"].toString();
        e.arguments              = toStringList(o["arguments"].toArray());
        e.startupPolicy          = o["startupPolicy"].toString("manual");
        e.closePolicy            = o["closePolicy"].toString("terminateThenKill");
        e.expectedWindowTitle    = o["expectedWindowTitle"].toString();
        e.category               = o["category"].toString();
        e.linkedAndroidId        = o["linkedAndroidId"].toString();
        m_apps.append(e);
    }
    return true;
}

bool AppConfig::saveToFile(const QString& path) const {
    QJsonArray appsArray;
    for (const auto& e : m_apps) {
        QJsonObject o;
        o["id"]                     = e.id;
        o["name"]                   = e.name;
        o["description"]            = e.description;
        o["executable"]             = e.executable;
        o["workingDirectory"]       = e.workingDirectory;
        o["arguments"]              = toJsonArray(e.arguments);
        o["startupPolicy"]          = e.startupPolicy;
        o["closePolicy"]            = e.closePolicy;
        o["expectedWindowTitle"]    = e.expectedWindowTitle;
        o["category"]               = e.category;
        if (!e.linkedAndroidId.isEmpty())
            o["linkedAndroidId"]    = e.linkedAndroidId;
        appsArray.append(o);
    }

    QJsonObject root;
    root["version"] = m_version;
    root["apps"]    = appsArray;

    QDir().mkpath(QFileInfo(path).dir().absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool AppConfig::copyDefaultTo(const QString& destPath) {
    QFile src(":/defaults/resources/apps_default.json");
    if (!src.open(QIODevice::ReadOnly))
        return false;

    QDir().mkpath(QFileInfo(destPath).dir().absolutePath());

    QFile dst(destPath);
    if (!dst.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    dst.write(src.readAll());
    return true;
}
