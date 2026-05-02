#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include "AppConfig.h"
#include "AppManager.h"
#include "MediaConfig.h"
#include "MediaManager.h"

class ConfigureModeScreen : public QWidget {
    Q_OBJECT
public:
    explicit ConfigureModeScreen(const QString& packageRoot, QWidget* parent = nullptr);
    ~ConfigureModeScreen() override = default;

signals:
    void returnToSelector();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onStateChanged(const QString& id, AppState state);
    void onMediaStateChanged(const QString& id, MediaState state);
    void onLogMessage(const QString& formatted);

private:
    void buildUI();

    // Apps
    void loadConfig();
    void populateTable();
    void updateRow(const QString& id);
    int  rowForId(const QString& id) const;
    int  configIndexForId(const QString& id) const;
    void addApp();
    void editApp(const QString& id);
    void deleteApp(const QString& id);
    void applyConfigChanges();

    // Media
    void loadMediaConfig();
    void populateMediaTable();
    void updateMediaRow(const QString& id);
    int  mediaRowForId(const QString& id) const;
    int  mediaConfigIndexForId(const QString& id) const;
    void addMedia();
    void editMedia(const QString& id);
    void deleteMedia(const QString& id);
    void applyMediaChanges();

    QString       m_packageRoot;

    QString       m_configPath;
    AppConfig     m_config;
    AppManager*   m_manager      = nullptr;

    QString       m_mediaConfigPath;
    MediaConfig   m_mediaConfig;
    MediaManager* m_mediaManager = nullptr;

    QTableWidget* m_table            = nullptr;
    QPushButton*  m_addBtn           = nullptr;
    QPushButton*  m_stopAllBtn       = nullptr;

    QTableWidget* m_mediaTable       = nullptr;
    QPushButton*  m_addMediaBtn      = nullptr;
    QPushButton*  m_stopAllMediaBtn  = nullptr;

    QTextEdit*    m_logPanel         = nullptr;
};
