#include "MediaManager.h"
#include "WorkspacePaths.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QWindow>
#include <QGuiApplication>
#include <QScreen>
#include <QFileInfo>
#include <QDir>
#include <algorithm>

MediaManager::MediaManager(const QString& packageRoot, QObject* parent)
    : QObject(parent)
    , m_workspaceRoot(WorkspacePaths::findWorkspaceRoot(packageRoot)) {}

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

void MediaManager::setStageGeometry(const QRect& geo) {
    m_stageGeometry = geo;
}

QString MediaManager::resolve(const QString& path) const {
    if (QFileInfo(path).isAbsolute())
        return path;
    return QDir(m_workspaceRoot).filePath(path);
}

void MediaManager::ensurePlayer(const QString& id, const MediaEntry& /*entry*/) {
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
}

void MediaManager::play(const QString& id) {
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&](const MediaEntry& e) { return e.id == id; });
    if (it == m_entries.end()) return;
    const MediaEntry& entry = *it;

    auto& rt = m_runtimes[id];
    if (rt.state == MediaState::Playing) return;

    ensurePlayer(id, entry);

    const QString mediaPath = resolve(entry.path);

    if (!QFileInfo::exists(mediaPath)) {
        emit logMessage(QString("ERROR: Archivo no encontrado: %1").arg(mediaPath));
        setState(id, MediaState::Error);
        return;
    }

    if (entry.type == "video") {
        const bool wantStage = !m_stageGeometry.isEmpty();
        // Recreate the window if we switch between stage playback and single-screen rehearsal,
        // because they use different top-level flags.
        const bool hasStageWgt = rt.videoWidget &&
                                 (rt.videoWidget->windowFlags() & Qt::WindowStaysOnTopHint);
        if (rt.videoWidget && wantStage != bool(hasStageWgt)) {
            rt.videoWidget->close();
            delete rt.videoWidget;
            rt.videoWidget = nullptr;
        }

        if (wantStage) {
            if (!rt.videoWidget) {
                rt.videoWidget = new QVideoWidget();
                rt.videoWidget->setWindowFlags(
                    Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
                rt.videoWidget->setAttribute(Qt::WA_DeleteOnClose, false);
                rt.videoWidget->setStyleSheet("background: black;");
            }
            // Pin to the correct screen before showing (same fix as StageWindow).
            QScreen* targetScreen = nullptr;
            for (auto* s : QGuiApplication::screens())
                if (s->geometry() == m_stageGeometry) { targetScreen = s; break; }
            rt.videoWidget->winId(); // forces native window creation
            if (rt.videoWidget->windowHandle() && targetScreen)
                rt.videoWidget->windowHandle()->setScreen(targetScreen);

            rt.videoWidget->setGeometry(m_stageGeometry);
            rt.player->setVideoOutput(rt.videoWidget);
            rt.videoWidget->show();
            rt.videoWidget->raise();
        } else {
            // Fallback: fullscreen on the primary screen for single-screen rehearsal.
            if (!rt.videoWidget) {
                rt.videoWidget = new QVideoWidget();
                rt.videoWidget->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
                rt.videoWidget->setStyleSheet("background: black;");
                rt.videoWidget->setAttribute(Qt::WA_DeleteOnClose, false);
            }
            QScreen* primary = QGuiApplication::primaryScreen();
            if (primary) {
                rt.videoWidget->winId();
                if (rt.videoWidget->windowHandle())
                    rt.videoWidget->windowHandle()->setScreen(primary);
                rt.videoWidget->setGeometry(primary->geometry());
                rt.videoWidget->showFullScreen();
            } else {
                rt.videoWidget->resize(1280, 720);
                rt.videoWidget->show();
            }
            rt.player->setVideoOutput(rt.videoWidget);
            rt.videoWidget->raise();
        }
    }

    emit logMessage(QString("Reproduciendo %1...").arg(entry.name));
    emit logMessage(QString("  Archivo: %1").arg(mediaPath));
    rt.player->setSource(QUrl::fromLocalFile(mediaPath));
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
