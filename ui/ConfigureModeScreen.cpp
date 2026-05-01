#include "ConfigureModeScreen.h"
#include "Logger.h"
#include "CyberTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QKeyEvent>
#include <QShowEvent>

static QString stateLabel(AppState s) {
    switch (s) {
        case AppState::Stopped:  return "DETENIDA";
        case AppState::Starting: return "LANZANDO...";
        case AppState::Running:  return "EJECUTÁNDOSE";
        case AppState::Stopping: return "DETENIENDO...";
        case AppState::Error:    return "ERROR";
    }
    return {};
}

static QColor stateColor(AppState s) {
    switch (s) {
        case AppState::Stopped:  return CyberTheme::color(CyberTheme::TextMuted);
        case AppState::Starting: return CyberTheme::color(CyberTheme::Warning);
        case AppState::Running:  return CyberTheme::color(CyberTheme::AccentGreen);
        case AppState::Stopping: return CyberTheme::color(CyberTheme::Warning);
        case AppState::Error:    return CyberTheme::color(CyberTheme::Error);
    }
    return {};
}

ConfigureModeScreen::ConfigureModeScreen(const QString& packageRoot, QWidget* parent)
    : QWidget(parent)
    , m_packageRoot(packageRoot)
    , m_manager(new AppManager(packageRoot, this))
{
    setFocusPolicy(Qt::StrongFocus);

    connect(m_manager, &AppManager::logMessage,
            &Logger::instance(), &Logger::log);
    connect(&Logger::instance(), &Logger::messageLogged,
            this, &ConfigureModeScreen::onLogMessage);
    connect(m_manager, &AppManager::stateChanged,
            this, &ConfigureModeScreen::onStateChanged);

    buildUI();
    loadConfig();
    populateTable();
}

void ConfigureModeScreen::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(16, 14, 16, 14);

    // Header
    auto* header = new QHBoxLayout();
    auto* title = new QLabel("CONFIGURAR", this);
    title->setObjectName("ScreenTitle");
    header->addWidget(title);
    header->addStretch();

    auto* escHint = new QLabel("Esc · Volver", this);
    escHint->setObjectName("MutedLabel");
    header->addWidget(escHint);

    root->addLayout(header);

    // App table
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({"Aplicación", "Estado", "Iniciar", "Parar", "Reiniciar"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_table->setColumnWidth(1, 110);
    for (int c = 2; c <= 4; ++c) {
        m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
        m_table->setColumnWidth(c, 82);
    }
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setStyleSheet(QStringLiteral(R"QSS(
        QTableWidget {
            background-color: #101318;
            border: 1px solid #293241;
            border-radius: 8px;
            gridline-color: #1A2030;
        }
        QTableWidget::item {
            padding: 4px 8px;
            border-bottom: 1px solid #1A2030;
        }
        QHeaderView::section {
            background-color: #151922;
            color: #8D96A3;
            border: none;
            border-bottom: 1px solid #293241;
            padding: 6px 8px;
            font-weight: 700;
            font-size: 11px;
            letter-spacing: 0.5px;
        }
    )QSS"));
    root->addWidget(m_table, 3);

    // Stop All
    auto* actionsBar = new QHBoxLayout();
    actionsBar->addStretch();
    m_stopAllBtn = new QPushButton("Parar todo", this);
    m_stopAllBtn->setObjectName("DangerButton");
    m_stopAllBtn->setMinimumWidth(110);
    m_stopAllBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_stopAllBtn, &QPushButton::clicked, m_manager, &AppManager::stopAll);
    actionsBar->addWidget(m_stopAllBtn);
    root->addLayout(actionsBar);

    // Log panel
    auto* logLabel = new QLabel("Log:", this);
    logLabel->setObjectName("FieldLabel");
    root->addWidget(logLabel);

    m_logPanel = new QTextEdit(this);
    m_logPanel->setReadOnly(true);
    m_logPanel->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  background-color: #0A0E14;"
        "  border: 1px solid #293241;"
        "  border-radius: 8px;"
        "  color: #00FF55;"
        "  font-family: 'Consolas', 'JetBrains Mono', monospace;"
        "  font-size: 11px;"
        "  padding: 8px;"
        "}"
    ));
    root->addWidget(m_logPanel, 1);
}

void ConfigureModeScreen::loadConfig() {
    QString configPath = QDir(m_packageRoot).filePath("config/apps.json");

    if (!QFileInfo::exists(configPath)) {
        Logger::instance().log("config/apps.json not found — creating default.");
        if (!AppConfig::copyDefaultTo(configPath))
            Logger::instance().log("WARNING: Could not write default config.");
    }

    if (!m_config.loadFromFile(configPath)) {
        Logger::instance().log("ERROR: Failed to parse config/apps.json.");
        QMessageBox::critical(this, "Config Error",
            "Could not load config/apps.json.\nCheck the log for details.");
        return;
    }

    QList<AppEntry> enabled;
    for (const auto& e : m_config.apps())
        if (e.enabled) enabled.append(e);

    m_manager->loadApps(enabled);
    Logger::instance().log(QString("Config loaded: %1 app(s).").arg(enabled.size()));
}

void ConfigureModeScreen::populateTable() {
    m_table->setRowCount(0);
    const auto& entries = m_manager->entries();

    for (int row = 0; row < entries.size(); ++row) {
        const AppEntry& e = entries[row];
        m_table->insertRow(row);

        auto* nameItem = new QTableWidgetItem(e.name);
        nameItem->setForeground(CyberTheme::color(CyberTheme::TextPrimary));
        m_table->setItem(row, 0, nameItem);

        auto* stateItem = new QTableWidgetItem(stateLabel(AppState::Stopped));
        stateItem->setForeground(stateColor(AppState::Stopped));
        m_table->setItem(row, 1, stateItem);

        auto* startBtn = new QPushButton("Iniciar",   this);
        auto* stopBtn  = new QPushButton("Parar",     this);
        auto* rstBtn   = new QPushButton("Reiniciar", this);
        startBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setFocusPolicy(Qt::NoFocus);
        rstBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setEnabled(false);
        rstBtn->setEnabled(false);

        const QString id = e.id;
        connect(startBtn, &QPushButton::clicked, this, [this, id]() { m_manager->start(id);   });
        connect(stopBtn,  &QPushButton::clicked, this, [this, id]() { m_manager->stop(id);    });
        connect(rstBtn,   &QPushButton::clicked, this, [this, id]() { m_manager->restart(id); });

        m_table->setCellWidget(row, 2, startBtn);
        m_table->setCellWidget(row, 3, stopBtn);
        m_table->setCellWidget(row, 4, rstBtn);
        m_table->setRowHeight(row, 38);
    }
}

int ConfigureModeScreen::rowForId(const QString& id) const {
    const auto& entries = m_manager->entries();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].id == id) return i;
    return -1;
}

void ConfigureModeScreen::updateRow(const QString& id) {
    int row = rowForId(id);
    if (row < 0) return;

    AppState s = m_manager->state(id);

    if (auto* item = m_table->item(row, 1)) {
        item->setText(stateLabel(s));
        item->setForeground(stateColor(s));
    }

    auto* startBtn = qobject_cast<QPushButton*>(m_table->cellWidget(row, 2));
    auto* stopBtn  = qobject_cast<QPushButton*>(m_table->cellWidget(row, 3));
    auto* rstBtn   = qobject_cast<QPushButton*>(m_table->cellWidget(row, 4));

    if (startBtn) startBtn->setEnabled(s == AppState::Stopped || s == AppState::Error);
    if (stopBtn)  stopBtn->setEnabled(s == AppState::Running   || s == AppState::Starting);
    if (rstBtn)   rstBtn->setEnabled(s == AppState::Running    || s == AppState::Error);
}

void ConfigureModeScreen::onStateChanged(const QString& id, AppState) {
    updateRow(id);
}

void ConfigureModeScreen::onLogMessage(const QString& formatted) {
    m_logPanel->append(formatted);
    m_logPanel->verticalScrollBar()->setValue(
        m_logPanel->verticalScrollBar()->maximum());
}

void ConfigureModeScreen::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape)
        emit returnToSelector();
    else
        QWidget::keyPressEvent(event);
}

void ConfigureModeScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setFocus();
}
