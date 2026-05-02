#pragma once
#include <QObject>
#include <QMap>
#include <QProcess>
#include <QTimer>
#include "AppConfig.h"

enum class AppState { Stopped, Starting, Running, Stopping, Error };
enum class ShowMode  { Configure, Design, Show };

struct AppRuntime {
    QProcess* process        = nullptr;
    AppState  state          = AppState::Stopped;
    QTimer*   killTimer      = nullptr;
    bool      pendingRestart = false;
};

class AppManager : public QObject {
    Q_OBJECT
public:
    explicit AppManager(const QString& packageRoot, QObject* parent = nullptr);

    void setMode(ShowMode mode);
    void loadApps(const QList<AppEntry>& apps);

    void start(const QString& id);
    void stop(const QString& id);
    void restart(const QString& id);
    void stopAll();

    AppState                state(const QString& id) const;
    const QList<AppEntry>&  entries() const { return m_entries; }

signals:
    void stateChanged(const QString& id, AppState state);
    void logMessage(const QString& message);

private:
    void    setState(const QString& id, AppState newState);
    QString resolve(const QString& relativePath) const;
    QStringList argsFor(const AppEntry& e) const;

    QString                   m_packageRoot;
    ShowMode                  m_mode = ShowMode::Configure;
    QList<AppEntry>           m_entries;
    QMap<QString, AppRuntime> m_runtimes;

    static constexpr int KILL_TIMEOUT_MS = 3000;
};
