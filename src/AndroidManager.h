#pragma once
#include <QObject>
#include <QMap>
#include "AndroidConfig.h"
#include "AdbManager.h"

enum class AndroidState { Stopped, Foreground, Background };

class AndroidManager : public QObject {
    Q_OBJECT
public:
    explicit AndroidManager(AdbManager* adb, QObject* parent = nullptr);

    void loadApps(const QList<AndroidEntry>& apps);

    void start(const QString& id);
    void stop(const QString& id);
    void bringToFront(const QString& id);
    void sendToBackground(const QString& id);
    void stopAll();

    AndroidState               state(const QString& id) const;
    const QList<AndroidEntry>& entries() const { return m_entries; }

signals:
    void stateChanged(const QString& id, AndroidState state);
    void logMessage(const QString& message);

private:
    void setState(const QString& id, AndroidState s);

    AdbManager*                 m_adb;
    QList<AndroidEntry>         m_entries;
    QMap<QString, AndroidState> m_states;
};
