#include "AppManager.h"
#include <QDir>
#include <QFileInfo>
#include <algorithm>

AppManager::AppManager(const QString& packageRoot, QObject* parent)
    : QObject(parent), m_packageRoot(packageRoot) {}

void AppManager::setMode(ShowMode mode) { m_mode = mode; }

void AppManager::loadApps(const QList<AppEntry>& apps) {
    m_entries = apps;
    for (const auto& e : apps)
        if (!m_runtimes.contains(e.id))
            m_runtimes[e.id] = AppRuntime{};
}

AppState AppManager::state(const QString& id) const {
    auto it = m_runtimes.find(id);
    return it != m_runtimes.end() ? it->state : AppState::Stopped;
}

void AppManager::setState(const QString& id, AppState newState) {
    m_runtimes[id].state = newState;
    emit stateChanged(id, newState);
}

QString AppManager::resolve(const QString& relativePath) const {
    return QDir(m_packageRoot).filePath(relativePath);
}

QStringList AppManager::argsFor(const AppEntry& e) const {
    QStringList args;
    switch (m_mode) {
        case ShowMode::Configure: args << "--configure"; args += e.configurationArguments; break;
        case ShowMode::Design:    args << "--design";    args += e.rehearsalArguments;     break;
        case ShowMode::Show:      args << "--show";      args += e.liveArguments;          break;
    }
    args += e.arguments;
    return args;
}

void AppManager::start(const QString& id) {
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
                           [&](const AppEntry& e) { return e.id == id; });
    if (it == m_entries.end()) return;
    const AppEntry& entry = *it;

    auto& rt = m_runtimes[id];
    if (rt.state != AppState::Stopped && rt.state != AppState::Error) return;

    QString workDir = resolve(entry.workingDirectory);
    QString exePath = QDir(workDir).filePath(entry.executable);

    if (!QFileInfo::exists(exePath)) {
        emit logMessage(QString("ERROR: Executable not found: %1").arg(exePath));
        setState(id, AppState::Error);
        return;
    }

    if (rt.process) {
        rt.process->deleteLater();
        rt.process = nullptr;
    }

    rt.process = new QProcess(this);
    rt.process->setWorkingDirectory(workDir);
    rt.process->setProgram(exePath);

    connect(rt.process, &QProcess::started, this, [this, id]() {
        setState(id, AppState::Running);
        auto it = std::find_if(m_entries.begin(), m_entries.end(),
                               [&](const AppEntry& e) { return e.id == id; });
        QString name = (it != m_entries.end()) ? it->name : id;
        emit logMessage(QString("%1 is running.").arg(name));
    });

    connect(rt.process, &QProcess::finished, this,
            [this, id](int exitCode, QProcess::ExitStatus status) {
        auto& rt = m_runtimes[id];
        if (rt.killTimer) {
            rt.killTimer->stop();
            rt.killTimer->deleteLater();
            rt.killTimer = nullptr;
        }

        auto it = std::find_if(m_entries.begin(), m_entries.end(),
                               [&](const AppEntry& e) { return e.id == id; });
        QString name = (it != m_entries.end()) ? it->name : id;

        if (rt.state == AppState::Stopping) {
            emit logMessage(QString("%1 stopped.").arg(name));
            setState(id, AppState::Stopped);
        } else if (status == QProcess::CrashExit) {
            if (rt.state != AppState::Error)
                setState(id, AppState::Error);
        } else {
            emit logMessage(QString("%1 exited unexpectedly (code %2).").arg(name).arg(exitCode));
            setState(id, AppState::Error);
        }

        if (rt.pendingRestart) {
            rt.pendingRestart = false;
            QTimer::singleShot(200, this, [this, id]() { start(id); });
        }
    });

    connect(rt.process, &QProcess::errorOccurred, this,
            [this, id](QProcess::ProcessError error) {
        auto& rt = m_runtimes[id];
        if (rt.state == AppState::Stopping) return;

        auto it = std::find_if(m_entries.begin(), m_entries.end(),
                               [&](const AppEntry& e) { return e.id == id; });
        QString name = (it != m_entries.end()) ? it->name : id;

        QString reason;
        switch (error) {
            case QProcess::FailedToStart: reason = "failed to start"; break;
            case QProcess::Crashed:       reason = "crashed";         break;
            case QProcess::Timedout:      reason = "timed out";       break;
            case QProcess::WriteError:    reason = "write error";     break;
            case QProcess::ReadError:     reason = "read error";      break;
            default:                      reason = "unknown error";   break;
        }
        emit logMessage(QString("ERROR: %1 — %2.").arg(name, reason));
        setState(id, AppState::Error);
    });

    QStringList args = argsFor(entry);
    rt.process->setArguments(args);

    setState(id, AppState::Starting);
    emit logMessage(QString("Starting %1...").arg(entry.name));
    emit logMessage(QString("  Executable: %1").arg(exePath));
    emit logMessage(QString("  Working dir: %1").arg(workDir));
    if (!args.isEmpty())
        emit logMessage(QString("  Args: %1").arg(args.join(' ')));

    rt.process->start();
}

void AppManager::stop(const QString& id) {
    auto& rt = m_runtimes[id];
    if (!rt.process) return;
    if (rt.state == AppState::Stopped || rt.state == AppState::Stopping) return;

    auto it = std::find_if(m_entries.begin(), m_entries.end(),
                           [&](const AppEntry& e) { return e.id == id; });
    QString name = (it != m_entries.end()) ? it->name : id;

    setState(id, AppState::Stopping);
    emit logMessage(QString("Stopping %1...").arg(name));
    rt.process->terminate();

    rt.killTimer = new QTimer(this);
    rt.killTimer->setSingleShot(true);
    connect(rt.killTimer, &QTimer::timeout, this, [this, id, name]() {
        auto& rt = m_runtimes[id];
        if (rt.process && rt.process->state() != QProcess::NotRunning) {
            emit logMessage(QString("Force-killing %1 (no response to terminate).").arg(name));
            rt.process->kill();
        }
        if (rt.killTimer) {
            rt.killTimer->deleteLater();
            rt.killTimer = nullptr;
        }
    });
    rt.killTimer->start(KILL_TIMEOUT_MS);
}

void AppManager::restart(const QString& id) {
    auto& rt = m_runtimes[id];
    if (rt.state == AppState::Stopped || rt.state == AppState::Error) {
        start(id);
    } else {
        rt.pendingRestart = true;
        stop(id);
    }
}

void AppManager::stopAll() {
    emit logMessage("Stopping all applications...");
    for (const auto& e : m_entries)
        stop(e.id);
}
