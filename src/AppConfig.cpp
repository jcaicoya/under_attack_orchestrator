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
        e.enabled                = o["enabled"].toBool(true);
        e.executable             = o["executable"].toString();
        e.workingDirectory       = o["workingDirectory"].toString();
        e.arguments              = toStringList(o["arguments"].toArray());
        e.configurationArguments = toStringList(o["configurationArguments"].toArray());
        e.rehearsalArguments     = toStringList(o["rehearsalArguments"].toArray());
        e.liveArguments          = toStringList(o["liveArguments"].toArray());
        e.startupPolicy          = o["startupPolicy"].toString("manual");
        e.closePolicy            = o["closePolicy"].toString("terminateThenKill");
        e.expectedWindowTitle    = o["expectedWindowTitle"].toString();
        e.category               = o["category"].toString();
        m_apps.append(e);
    }
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
