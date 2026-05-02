#include "ConfigureModeScreen.h"
#include "Logger.h"
#include "CyberTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QFileInfo>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QKeyEvent>
#include <QShowEvent>
#include <QStyle>

static QString stateLabel(AppState s) {
    switch (s) {
        case AppState::Stopped:  return "LISTA";
        case AppState::Starting: return "EJECUTÁNDOSE";
        case AppState::Running:  return "EJECUTÁNDOSE";
        case AppState::Stopping: return "EJECUTÁNDOSE";
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

static QPushButton* makeIconButton(QStyle::StandardPixmap icon, const QString& tooltip, QWidget* parent) {
    auto* btn = new QPushButton(parent);
    btn->setIcon(parent->style()->standardIcon(icon));
    btn->setFixedSize(28, 28);
    btn->setToolTip(tooltip);
    btn->setFocusPolicy(Qt::NoFocus);
    return btn;
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

    // App table — 5 columns: Aplicación | Iniciar | Parar | Estado | (edit+del)
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({"Aplicación", "Iniciar", "Parar", "Estado", ""});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c <= 2; ++c) {
        m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Fixed);
        m_table->setColumnWidth(c, 90);
    }
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_table->setColumnWidth(3, 120);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_table->setColumnWidth(4, 68);

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

    // Action bar: Add (left) | stretch | Stop All (right)
    auto* actionsBar = new QHBoxLayout();

    m_addBtn = new QPushButton("+ Añadir", this);
    m_addBtn->setFocusPolicy(Qt::NoFocus);
    m_addBtn->setMinimumWidth(100);
    connect(m_addBtn, &QPushButton::clicked, this, &ConfigureModeScreen::addApp);
    actionsBar->addWidget(m_addBtn);

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
    m_configPath = QDir(m_packageRoot).filePath("config/apps.json");

    if (!QFileInfo::exists(m_configPath)) {
        Logger::instance().log("config/apps.json not found — creating default.");
        if (!AppConfig::copyDefaultTo(m_configPath))
            Logger::instance().log("WARNING: Could not write default config.");
    }

    if (!m_config.loadFromFile(m_configPath)) {
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

        auto* startBtn = new QPushButton("Iniciar", this);
        auto* stopBtn  = new QPushButton("Parar",   this);
        startBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setEnabled(false);

        const QString id = e.id;
        connect(startBtn, &QPushButton::clicked, this, [this, id]() { m_manager->start(id); });
        connect(stopBtn,  &QPushButton::clicked, this, [this, id]() { m_manager->stop(id);  });

        m_table->setCellWidget(row, 1, startBtn);
        m_table->setCellWidget(row, 2, stopBtn);

        auto* stateItem = new QTableWidgetItem(stateLabel(AppState::Stopped));
        stateItem->setForeground(stateColor(AppState::Stopped));
        m_table->setItem(row, 3, stateItem);

        // Edit + Delete buttons
        auto* actWidget = new QWidget(this);
        actWidget->setStyleSheet("background: transparent;");
        auto* actLayout = new QHBoxLayout(actWidget);
        actLayout->setContentsMargins(2, 2, 2, 2);
        actLayout->setSpacing(4);

        auto* editBtn = makeIconButton(QStyle::SP_FileDialogDetailedView, "Cambiar ejecutable", this);
        auto* delBtn  = makeIconButton(QStyle::SP_TrashIcon, "Eliminar", this);
        delBtn->setStyleSheet(
            "QPushButton { background: #2A1010; border: 1px solid #6A2020; border-radius: 4px; }"
            "QPushButton:hover { background: #3A1515; border-color: #AA3030; }"
            "QPushButton:pressed { background: #1A0808; }"
        );

        connect(editBtn, &QPushButton::clicked, this, [this, id]() { editApp(id); });
        connect(delBtn,  &QPushButton::clicked, this, [this, id]() { deleteApp(id); });

        actLayout->addWidget(editBtn);
        actLayout->addWidget(delBtn);
        m_table->setCellWidget(row, 4, actWidget);

        m_table->setRowHeight(row, 38);
    }
}

int ConfigureModeScreen::rowForId(const QString& id) const {
    const auto& entries = m_manager->entries();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].id == id) return i;
    return -1;
}

int ConfigureModeScreen::configIndexForId(const QString& id) const {
    const auto& apps = m_config.apps();
    for (int i = 0; i < apps.size(); ++i)
        if (apps[i].id == id) return i;
    return -1;
}

void ConfigureModeScreen::applyConfigChanges() {
    if (!m_config.saveToFile(m_configPath))
        Logger::instance().log("ERROR: Could not save config/apps.json.");

    QList<AppEntry> enabled;
    for (const auto& e : m_config.apps())
        if (e.enabled) enabled.append(e);
    m_manager->loadApps(enabled);
    populateTable();
}

void ConfigureModeScreen::addApp() {
    QString exePath = QFileDialog::getOpenFileName(
        this, "Seleccionar ejecutable", m_packageRoot, "Executables (*.exe)");
    if (exePath.isEmpty()) return;

    QFileInfo fi(exePath);
    AppEntry e;
    e.id               = fi.completeBaseName();
    e.name             = fi.completeBaseName();
    e.description      = fi.completeBaseName();
    e.executable       = fi.fileName();
    e.workingDirectory = fi.absolutePath();
    e.enabled          = true;

    auto reply = QMessageBox::question(this, "Añadir aplicación",
        QString("¿Añadir '%1'?\n\nEjecutable:  %2\nDirectorio: %3")
            .arg(e.name, e.executable, e.workingDirectory));
    if (reply != QMessageBox::Yes) return;

    m_config.addApp(e);
    applyConfigChanges();
    Logger::instance().log(QString("App añadida: %1").arg(e.name));
}

void ConfigureModeScreen::editApp(const QString& id) {
    int ci = configIndexForId(id);
    if (ci < 0) return;

    AppState s = m_manager->state(id);
    if (s != AppState::Stopped && s != AppState::Error) {
        QMessageBox::warning(this, "App en ejecución",
            QString("Para '%1' antes de modificarla.").arg(m_config.apps()[ci].name));
        return;
    }

    const AppEntry& current = m_config.apps()[ci];
    QString startDir = QFileInfo::exists(current.workingDirectory)
        ? current.workingDirectory : m_packageRoot;
    QString exePath = QFileDialog::getOpenFileName(
        this, "Seleccionar ejecutable", startDir, "Executables (*.exe)");
    if (exePath.isEmpty()) return;

    QFileInfo fi(exePath);
    AppEntry updated        = current;
    updated.executable      = fi.fileName();
    updated.workingDirectory = fi.absolutePath();

    auto reply = QMessageBox::question(this, "Actualizar aplicación",
        QString("¿Actualizar '%1'?\n\nEjecutable:  %2\nDirectorio: %3")
            .arg(updated.name, updated.executable, updated.workingDirectory));
    if (reply != QMessageBox::Yes) return;

    m_config.updateApp(ci, updated);
    applyConfigChanges();
    Logger::instance().log(QString("App actualizada: %1").arg(updated.name));
}

void ConfigureModeScreen::deleteApp(const QString& id) {
    int ci = configIndexForId(id);
    if (ci < 0) return;

    AppState s = m_manager->state(id);
    if (s != AppState::Stopped && s != AppState::Error) {
        QMessageBox::warning(this, "App en ejecución",
            QString("Para '%1' antes de eliminarla.").arg(m_config.apps()[ci].name));
        return;
    }

    const AppEntry& e = m_config.apps()[ci];
    auto reply = QMessageBox::question(this, "Eliminar aplicación",
        QString("¿Eliminar '%1'?\n\nSe eliminará de la configuración.").arg(e.name));
    if (reply != QMessageBox::Yes) return;

    QString name = e.name;
    m_config.removeApp(ci);
    applyConfigChanges();
    Logger::instance().log(QString("App eliminada: %1").arg(name));
}

void ConfigureModeScreen::updateRow(const QString& id) {
    int row = rowForId(id);
    if (row < 0) return;

    AppState s = m_manager->state(id);

    auto* startBtn = qobject_cast<QPushButton*>(m_table->cellWidget(row, 1));
    auto* stopBtn  = qobject_cast<QPushButton*>(m_table->cellWidget(row, 2));

    if (auto* item = m_table->item(row, 3)) {
        item->setText(stateLabel(s));
        item->setForeground(stateColor(s));
    }

    bool canStart = (s == AppState::Stopped || s == AppState::Error);
    bool canStop  = (s == AppState::Starting || s == AppState::Running || s == AppState::Stopping);
    if (startBtn) startBtn->setEnabled(canStart);
    if (stopBtn)  stopBtn->setEnabled(canStop);
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
