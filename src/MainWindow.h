#pragma once
#include <QMainWindow>
#include <QStackedWidget>
#include "ui/ModeSelectorScreen.h"
#include "ui/ConfigureModeScreen.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& packageRoot, QWidget* parent = nullptr);
    ~MainWindow() override = default;

private:
    QString              m_packageRoot;
    QStackedWidget*      m_stack            = nullptr;
    ModeSelectorScreen*  m_selectorScreen   = nullptr;
    ConfigureModeScreen* m_configureScreen  = nullptr;
};
