#include "MediaManager.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QFileInfo>
#include <algorithm>

MediaManager::MediaManager(QObject* parent) : QObject(parent) {}

MediaManager::~MediaManager() {
    for (auto& rt : m_runtimes) {
        if (rt.player) rt.player->stop();
        if (rt.videoWidget) {
            rt.videoWidget->close();
            delete rt.videoWidget;
        }
    }
}

void MediaManager::loadMedia(const QList<MediaEntry>& items) {
    m_entries = items;
    for (const auto& e : items)
        if (!m_runtimes.contains(e.id))
            m_runtimes[e.id] = MediaRuntime{};
}

MediaState MediaManager::state(const QString& id) const {
    auto it = m_runtimes.find(id);
    return it != m_runtimes.end() ? it->state : MediaState::Stopped;
}

void MediaManager::setState(const QString& id, MediaState newState) {
    m_runtimes[id].state = newState;
    emit stateChanged(id, newState);
}

void MediaManager::ensurePlayer(const QString& id, const MediaEntry& entry) {
    auto& rt = m_runtimes[id];
    if (rt.player) return;

    rt.player = new QMediaPlayer(this);
    rt.audioOutput = new QAudioOutput(this);
    rt.player->setAudioOutput(rt.audioOutput);

    connect(rt.player, &QMediaPlayer::playbackStateChanged, this,
        [this, id](QMediaPlayer::PlaybackState s) {
            auto it = std::find_if(m_entries.begin(), m_entries.end(),
                [&](const MediaEntry& e) { return e.id == id; });
            QString name = (it != m_entries.end()) ? it->name : id;

            if (s == QMediaPlayer::PlayingState) {
                emit logMessage(QString("%1 reproduciendo.").arg(name));
                setState(id, MediaState::Playing);
            } else if (s == QMediaPlayer::StoppedState) {
                auto& rt = m_runtimes[id];
                if (rt.videoWidget) rt.videoWidget->hide();
                if (rt.state != MediaState::Error) {
                    emit logMessage(QString("%1 parado.").arg(name));
                    setState(id, MediaState::Stopped);
                }
            }
        });

    connect(rt.player, &QMediaPlayer::errorOccurred, this,
        [this, id](QMediaPlayer::Error, const QString& errorString) {
            auto it = std::find_if(m_entries.begin(), m_entries.end(),
                [&](const MediaEntry& e) { return e.id == id; });
            QString name = (it != m_entries.end()) ? it->name : id;
            emit logMessage(QString("ERROR: %1 — %2").arg(name, errorString));
            setState(id, MediaState::Error);
        });

    if (entry.type == "video") {
        if (m_stageOutput) {
            rt.player->setVideoOutput(m_stageOutput);
        } else {
            rt.videoWidget = new QVideoWidget();   // parentless = top-level fallback
            rt.videoWidget->setWindowTitle(entry.name);
            rt.videoWidget->setAttribute(Qt::WA_DeleteOnClose, false);
            rt.videoWidget->resize(854, 480);
            rt.player->setVideoOutput(rt.videoWidget);
        }
    }
}

void MediaManager::play(const QString& id) {
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&](const MediaEntry& e) { return e.id == id; });
    if (it == m_entries.end()) return;
    const MediaEntry& entry = *it;

    auto& rt = m_runtimes[id];
    if (rt.state == MediaState::Playing) return;

    ensurePlayer(id, entry);

    if (!QFileInfo::exists(entry.path)) {
        emit logMessage(QString("ERROR: Archivo no encontrado: %1").arg(entry.path));
        setState(id, MediaState::Error);
        return;
    }

    emit logMessage(QString("Reproduciendo %1...").arg(entry.name));
    emit logMessage(QString("  Archivo: %1").arg(entry.path));

    if (rt.videoWidget) {
        rt.videoWidget->show();
        rt.videoWidget->raise();
    }
    // If m_stageOutput is set, the caller switches the stage to Video mode
    // via the stateChanged(Playing) signal — no action needed here.

    rt.player->setSource(QUrl::fromLocalFile(entry.path));
    rt.player->play();
}

void MediaManager::stop(const QString& id) {
    auto& rt = m_runtimes[id];
    if (!rt.player || rt.state == MediaState::Stopped) return;
    rt.player->stop();
    if (rt.videoWidget) rt.videoWidget->hide();
}

void MediaManager::stopAll() {
    emit logMessage("Parando todos los medios...");
    for (const auto& e : m_entries)
        stop(e.id);
}

void MediaManager::setStageOutput(QVideoWidget* widget) {
    m_stageOutput = widget;
    for (auto it = m_runtimes.begin(); it != m_runtimes.end(); ++it) {
        MediaRuntime& rt = it.value();
        if (!rt.player) continue;
        const QString& id = it.key();
        auto entry = std::find_if(m_entries.begin(), m_entries.end(),
            [&](const MediaEntry& e) { return e.id == id; });
        if (entry == m_entries.end() || entry->type != "video") continue;

        if (m_stageOutput) {
            rt.player->setVideoOutput(m_stageOutput);
            if (rt.videoWidget) {
                rt.videoWidget->hide();
                delete rt.videoWidget;
                rt.videoWidget = nullptr;
            }
        } else {
            if (!rt.videoWidget) {
                rt.videoWidget = new QVideoWidget();
                rt.videoWidget->setWindowTitle(entry->name);
                rt.videoWidget->setAttribute(Qt::WA_DeleteOnClose, false);
                rt.videoWidget->resize(854, 480);
                rt.player->setVideoOutput(rt.videoWidget);
            }
        }
    }
}
