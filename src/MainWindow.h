#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "StageWindow.h"
#include "ui/ModeSelectorScreen.h"
#include "ui/ConfigureModeScreen.h"
#include "ui/RehearsalModeScreen.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& packageRoot, QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    QString               m_packageRoot;
    QStackedWidget*       m_stack            = nullptr;
    StageWindow*          m_stageWindow      = nullptr;
    ModeSelectorScreen*   m_selectorScreen   = nullptr;
    ConfigureModeScreen*  m_configureScreen  = nullptr;
    RehearsalModeScreen*  m_rehearsalScreen  = nullptr;
};
