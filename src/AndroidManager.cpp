#include "AndroidManager.h"
#include <algorithm>

AndroidManager::AndroidManager(AdbManager* adb, QObject* parent)
    : QObject(parent), m_adb(adb) {}

void AndroidManager::loadApps(const QList<AndroidEntry>& apps) {
    m_entries = apps;
    for (const auto& e : apps)
        if (!m_states.contains(e.id))
            m_states[e.id] = AndroidState::Stopped;
}

AndroidState AndroidManager::state(const QString& id) const {
    return m_states.value(id, AndroidState::Stopped);
}

void AndroidManager::setState(const QString& id, AndroidState s) {
    m_states[id] = s;
    emit stateChanged(id, s);
}

void AndroidManager::start(const QString& id) {
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
                           [&](const AndroidEntry& e) { return e.id == id; });
    if (it == m_entries.end()) return;

    emit logMessage(QString("Starting Android: %1 (%2)").arg(it->name, it->package));
    if (it->wsPort > 0)
        m_adb->setupReverseTunnel(it->wsPort);
    m_adb->launchApp(it->package, it->activity);
    setState(id, AndroidState::Running);
}

void AndroidManager::stop(const QString& id) {
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
                           [&](const AndroidEntry& e) { return e.id == id; });
    if (it == m_entries.end()) return;

    emit logMessage(QString("Stopping Android: %1").arg(it->name));
    m_adb->stopApp(it->package);
    setState(id, AndroidState::Stopped);
}

void AndroidManager::stopAll() {
    emit logMessage("Stopping all Android apps...");
    for (const auto& e : m_entries)
        stop(e.id);
}
