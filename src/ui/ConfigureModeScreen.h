#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include "AppConfig.h"
#include "AppManager.h"
#include "MediaConfig.h"
#include "MediaManager.h"
#include "StageWindow.h"

class ConfigureModeScreen : public QWidget {
    Q_OBJECT
public:
    explicit ConfigureModeScreen(const QString& packageRoot, QWidget* parent = nullptr);
    ~ConfigureModeScreen() override = default;

    void setStageWindow(StageWindow* stage);

signals:
    void returnToSelector();
    void switchMode(int mode);   // 0=Configure 1=Ensayo 2=Show

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onStateChanged(const QString& id, AppState state);
    void onMediaStateChanged(const QString& id, MediaState state);
    void onLogMessage(const QString& formatted);

private:
    void buildUI();

    // Stage
    void populateScreenCombo();
    void onActivateStage();
    void updateStageStatus();
    void loadStageConfig();
    void saveStageConfig(int screenIndex);

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

    // Stage
    StageWindow*  m_stageWindow       = nullptr;
    QComboBox*    m_screenCombo       = nullptr;
    QPushButton*  m_stageActivateBtn  = nullptr;
    QPushButton*  m_stageBlackBtn     = nullptr;
    QPushButton*  m_stageLogoBtn      = nullptr;
    QLabel*       m_stageStatusLabel  = nullptr;

    QString       m_configPath;
    AppConfig     m_config;
    AppManager*   m_manager           = nullptr;

    QString       m_mediaConfigPath;
    MediaConfig   m_mediaConfig;
    MediaManager* m_mediaManager      = nullptr;

    QTableWidget* m_table             = nullptr;
    QPushButton*  m_addBtn            = nullptr;
    QPushButton*  m_stopAllBtn        = nullptr;

    QTableWidget* m_mediaTable        = nullptr;
    QPushButton*  m_addMediaBtn       = nullptr;
    QPushButton*  m_stopAllMediaBtn   = nullptr;

    QTextEdit*    m_logPanel          = nullptr;
};
