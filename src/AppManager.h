#pragma once
#include <QObject>
#include <QMap>
#include <QProcess>
#include <QTimer>
#include "AppConfig.h"

enum class AppState { Stopped, Starting, Running, Stopping, Error };
enum class AppLaunchMode { Demo, Live };

struct AppRuntime {
    QProcess* process        = nullptr;
    AppState  state          = AppState::Stopped;
    QTimer*   killTimer      = nullptr;
    bool      pendingRestart = false;
    AppLaunchMode pendingRestartMode = AppLaunchMode::Live;
};

class AppManager : public QObject {
    Q_OBJECT
public:
    explicit AppManager(const QString& packageRoot, QObject* parent = nullptr);

    void loadApps(const QList<AppEntry>& apps);
    void setStageScreen(int screenIndex);  // -1 = no stage

    void start(const QString& id, AppLaunchMode launchMode = AppLaunchMode::Live);
    void stop(const QString& id);
    void restart(const QString& id, AppLaunchMode launchMode = AppLaunchMode::Live);
    void stopAll();

    AppState                state(const QString& id) const;
    const QList<AppEntry>&  entries() const { return m_entries; }

signals:
    void stateChanged(const QString& id, AppState state);
    void logMessage(const QString& message);

private:
    void    setState(const QString& id, AppState newState);
    QString resolve(const QString& relativePath) const;
    QStringList argsFor(const AppEntry& e, AppLaunchMode launchMode) const;

    QString                   m_packageRoot;
    QList<AppEntry>           m_entries;
    QMap<QString, AppRuntime> m_runtimes;
    int                       m_stageScreenIndex = -1;

    static constexpr int KILL_TIMEOUT_MS = 3000;
};
