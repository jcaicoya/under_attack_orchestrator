#include "MainWindow.h"
#include "CyberBackgroundWidget.h"
#include <QVBoxLayout>

MainWindow::MainWindow(const QString& packageRoot, QWidget* parent)
    : QMainWindow(parent)
    , m_packageRoot(packageRoot)
{
    setWindowTitle("Cybershow Orchestrator");
    setMinimumSize(920, 620);

    auto* bg = new CyberBackgroundWidget(this);
    setCentralWidget(bg);

    auto* layout = new QVBoxLayout(bg);
    layout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(bg);
    layout->addWidget(m_stack);

    m_selectorScreen  = new ModeSelectorScreen(m_stack);
    m_configureScreen = new ConfigureModeScreen(packageRoot, m_stack);

    m_stack->addWidget(m_selectorScreen);   // index 0
    m_stack->addWidget(m_configureScreen);  // index 1

    connect(m_selectorScreen, &ModeSelectorScreen::modeConfirmed,
            this, [this](int index) {
                if (index == 0)
                    m_stack->setCurrentIndex(1);
            });

    connect(m_configureScreen, &ConfigureModeScreen::returnToSelector,
            this, [this]() {
                m_stack->setCurrentIndex(0);
            });

    m_stack->setCurrentIndex(0);
}
