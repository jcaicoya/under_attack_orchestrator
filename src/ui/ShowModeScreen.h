#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include "AppConfig.h"
#include "AppManager.h"
#include "MediaConfig.h"
#include "MediaManager.h"
#include "RundownConfig.h"
#include "StageWindow.h"

class ShowModeScreen : public QWidget {
    Q_OBJECT
public:
    explicit ShowModeScreen(const QString& packageRoot, QWidget* parent = nullptr);
    ~ShowModeScreen() override = default;

    void setStageWindow(StageWindow* stage);

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
    void loadAndSync();
    void populateTable();
    void updateRow(int row);
    void updateNavButtons();
    void updateStageControls();
    int  rowForRef(const QString& type, const QString& id) const;

    void activateScene(int row);
    void stopCurrentScene();

    const AppEntry*   appEntryForId(const QString& id) const;
    const MediaEntry* mediaEntryForId(const QString& id) const;

    QString        m_packageRoot;
    QString        m_rundownPath;

    AppConfig      m_appConfig;
    MediaConfig    m_mediaConfig;
    RundownConfig  m_rundownConfig;   // only enabled items are shown

    AppManager*    m_appManager   = nullptr;
    MediaManager*  m_mediaManager = nullptr;

    StageWindow*   m_stageWindow      = nullptr;
    QPushButton*   m_stageBlackBtn    = nullptr;
    QPushButton*   m_stageLogoBtn     = nullptr;
    QLabel*        m_stageStatusLabel = nullptr;

    QTableWidget*  m_table        = nullptr;
    QPushButton*   m_prevBtn      = nullptr;
    QPushButton*   m_nextBtn      = nullptr;
    QPushButton*   m_activateBtn  = nullptr;
    QPushButton*   m_stopAllBtn   = nullptr;
    QLabel*        m_sceneLabel   = nullptr;
    QTextEdit*     m_logPanel     = nullptr;

    int            m_currentRow   = -1;   // -1 = none active
};
