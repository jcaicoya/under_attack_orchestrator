#pragma once
#include <QObject>
#include <QMap>
#include "MediaConfig.h"

class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;

enum class MediaState { Stopped, Playing, Error };

class MediaManager : public QObject {
    Q_OBJECT
public:
    explicit MediaManager(QObject* parent = nullptr);
    ~MediaManager() override;

    void loadMedia(const QList<MediaEntry>& items);
    void setStageOutput(QVideoWidget* widget);

    MediaState state(const QString& id) const;
    void play(const QString& id);
    void stop(const QString& id);
    void stopAll();
    const QList<MediaEntry>& entries() const { return m_entries; }

signals:
    void stateChanged(const QString& id, MediaState state);
    void logMessage(const QString& msg);

private:
    void setState(const QString& id, MediaState newState);
    void ensurePlayer(const QString& id, const MediaEntry& entry);

    struct MediaRuntime {
        MediaState    state       = MediaState::Stopped;
        QMediaPlayer* player      = nullptr;
        QAudioOutput* audioOutput = nullptr;
        QVideoWidget* videoWidget = nullptr; // non-null only for video entries
    };

    QVideoWidget*               m_stageOutput = nullptr;
    QList<MediaEntry>           m_entries;
    QMap<QString, MediaRuntime> m_runtimes;
};
