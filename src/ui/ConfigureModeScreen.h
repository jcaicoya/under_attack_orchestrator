#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include "AppConfig.h"
#include "AppManager.h"
#include "AndroidConfig.h"
#include "AndroidManager.h"
#include "AdbManager.h"
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
    void onAppStateChanged(const QString& id, AppState state);
    void onAndroidStateChanged(const QString& id, AndroidState state);
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

    // Qt apps
    void loadAppsConfig();
    void populateAppsTable();
    void updateAppRow(const QString& id);
    int  appRowForId(const QString& id) const;
    int  appConfigIndexForId(const QString& id) const;
    void addApp();
    void editApp(const QString& id);
    void deleteApp(const QString& id);
    void applyAppsChanges();

    // Android apps
    void loadAndroidConfig();
    void populateAndroidTable();
    void updateAndroidRow(const QString& id);
    int  androidRowForId(const QString& id) const;
    int  androidConfigIndexForId(const QString& id) const;
    void addAndroidApp();
    void editAndroidApp(const QString& id);
    void deleteAndroidApp(const QString& id);
    void applyAndroidChanges();
    bool showAndroidDialog(AndroidEntry& entry, const QString& title);

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

    // Qt apps
    QString       m_appsConfigPath;
    AppConfig     m_appsConfig;
    AppManager*   m_appManager        = nullptr;
    QTableWidget* m_appsTable         = nullptr;

    // Android
    QString         m_androidConfigPath;
    AndroidConfig   m_androidConfig;
    AdbManager*     m_adb             = nullptr;
    AndroidManager* m_androidManager  = nullptr;
    QLabel*         m_adbStatusLabel  = nullptr;
    QTableWidget*   m_androidTable    = nullptr;

    // Media
    QString       m_mediaConfigPath;
    MediaConfig   m_mediaConfig;
    MediaManager* m_mediaManager      = nullptr;
    QTableWidget* m_mediaTable        = nullptr;

    QTextEdit*    m_logPanel          = nullptr;
};
