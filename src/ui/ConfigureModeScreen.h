#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include "AppConfig.h"
#include "AppManager.h"

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
    void onLogMessage(const QString& formatted);

private:
    void buildUI();
    void loadConfig();
    void populateTable();
    void updateRow(const QString& id);
    int  rowForId(const QString& id) const;
    int  configIndexForId(const QString& id) const;

    void addApp();
    void editApp(const QString& id);
    void deleteApp(const QString& id);
    void applyConfigChanges();

    QString      m_packageRoot;
    QString      m_configPath;
    AppConfig    m_config;
    AppManager*  m_manager;

    QTableWidget* m_table      = nullptr;
    QPushButton*  m_addBtn     = nullptr;
    QPushButton*  m_stopAllBtn = nullptr;
    QTextEdit*    m_logPanel   = nullptr;
};
