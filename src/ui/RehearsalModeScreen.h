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
#include "RundownConfig.h"
#include "StageWindow.h"

class RehearsalModeScreen : public QWidget {
    Q_OBJECT
public:
    explicit RehearsalModeScreen(const QString& packageRoot, QWidget* parent = nullptr);
    ~RehearsalModeScreen() override = default;

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
    void syncAndRefresh();

    // Rundown (Qt apps + media)
    void populateTable();
    void updateRow(int row);
    int  rowForRef(const QString& type, const QString& id) const;
    const AppEntry*   appEntryForId(const QString& id) const;
    const MediaEntry* mediaEntryForId(const QString& id) const;

    // Android apps
    void populateAndroidTable();
    void updateAndroidRow(const QString& id);
    int  androidRowForId(const QString& id) const;
    const AndroidEntry* androidEntryForId(const QString& id) const;

    // Stage
    void updateStageControls();
    void populateScreenCombo();
    void onActivateStage();
    void loadStageConfig();
    void saveStageConfig(int screenIndex);

    QString        m_packageRoot;
    QString        m_rundownPath;

    AppConfig      m_appConfig;
    AndroidConfig  m_androidConfig;
    MediaConfig    m_mediaConfig;
    RundownConfig  m_rundownConfig;

    AppManager*     m_appManager     = nullptr;
    AdbManager*     m_adb            = nullptr;
    AndroidManager* m_androidManager = nullptr;
    MediaManager*   m_mediaManager   = nullptr;

    // Stage
    StageWindow*   m_stageWindow      = nullptr;
    QComboBox*     m_screenCombo      = nullptr;
    QPushButton*   m_stageActivateBtn = nullptr;

    QLabel*        m_adbStatusLabel = nullptr;
    QTableWidget*  m_table         = nullptr;
    QTableWidget*  m_androidTable  = nullptr;
    QTextEdit*     m_logPanel      = nullptr;
};
