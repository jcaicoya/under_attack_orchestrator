#pragma once
#include <QMainWindow>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QButtonGroup>
#include "AppConfig.h"
#include "AppManager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& packageRoot, QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onStateChanged(const QString& id, AppState state);
    void onLogMessage(const QString& formatted);
    void onModeChanged(int modeIndex);

private:
    void buildUI();
    void loadConfig();
    void populateTable();
    void updateRow(const QString& id);
    int  rowForId(const QString& id) const;

    QString      m_packageRoot;
    AppConfig    m_config;
    AppManager*  m_manager;

    QButtonGroup* m_modeGroup  = nullptr;
    QTableWidget* m_table      = nullptr;
    QPushButton*  m_stopAllBtn = nullptr;
    QTextEdit*    m_logPanel   = nullptr;
};
