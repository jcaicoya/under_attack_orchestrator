#include "ConfigureModeScreen.h"
#include "DefaultConfigUtils.h"
#include "Logger.h"
#include "WorkspacePaths.h"
#include "CyberTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QScrollBar>
#include <QFileInfo>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QShowEvent>
#include <QStyle>
#include <QGuiApplication>
#include <QScreen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QTabWidget>

static QString appStateLabel(AppState s) {
    switch (s) {
        case AppState::Stopped:  return "LISTA";
        case AppState::Starting: return "EJECUTÁNDOSE";
        case AppState::Running:  return "EJECUTÁNDOSE";
        case AppState::Stopping: return "PARANDO";
        case AppState::Error:    return "ERROR";
    }
    return {};
}

static QColor appStateColor(AppState s) {
    switch (s) {
        case AppState::Stopped:  return CyberTheme::color(CyberTheme::TextMuted);
        case AppState::Starting: return CyberTheme::color(CyberTheme::Warning);
        case AppState::Running:  return CyberTheme::color(CyberTheme::AccentGreen);
        case AppState::Stopping: return CyberTheme::color(CyberTheme::Warning);
        case AppState::Error:    return CyberTheme::color(CyberTheme::Error);
    }
    return {};
}

static QString androidStateLabel(AndroidState s) {
    switch (s) {
        case AndroidState::Stopped: return "PARADA";
        case AndroidState::Running: return "EN MARCHA";
    }
    return {};
}

static QColor androidStateColor(AndroidState s) {
    switch (s) {
        case AndroidState::Stopped: return CyberTheme::color(CyberTheme::TextMuted);
        case AndroidState::Running: return CyberTheme::color(CyberTheme::AccentGreen);
    }
    return {};
}

static QString mediaStateLabel(MediaState s) {
    switch (s) {
        case MediaState::Stopped: return "LISTA";
        case MediaState::Playing: return "REPRODUCIENDO";
        case MediaState::Error:   return "ERROR";
    }
    return {};
}

static QColor mediaStateColor(MediaState s) {
    switch (s) {
        case MediaState::Stopped: return CyberTheme::color(CyberTheme::TextMuted);
        case MediaState::Playing: return CyberTheme::color(CyberTheme::AccentGreen);
        case MediaState::Error:   return CyberTheme::color(CyberTheme::Error);
    }
    return {};
}

static QString mediaTypeForPath(const QString& path) {
    static const QStringList video = {"mp4", "mov", "avi", "mkv", "webm"};
    return video.contains(QFileInfo(path).suffix().toLower()) ? "video" : "audio";
}

static QPushButton* makeIconButton(QStyle::StandardPixmap icon, const QString& tooltip, QWidget* parent) {
    auto* btn = new QPushButton(parent);
    btn->setIcon(parent->style()->standardIcon(icon));
    btn->setFixedSize(28, 28);
    btn->setToolTip(tooltip);
    btn->setFocusPolicy(Qt::NoFocus);
    return btn;
}

static QPushButton* makeDangerIconButton(QStyle::StandardPixmap icon, const QString& tooltip, QWidget* parent) {
    auto* btn = makeIconButton(icon, tooltip, parent);
    btn->setStyleSheet(
        "QPushButton { background: #2A1010; border: 1px solid #6A2020; border-radius: 4px; }"
        "QPushButton:hover { background: #3A1515; border-color: #AA3030; }"
        "QPushButton:pressed { background: #1A0808; }"
    );
    return btn;
}

static QWidget* makeActionCell(QPushButton* editBtn, QPushButton* delBtn, QWidget* parent) {
    auto* w = new QWidget(parent);
    w->setStyleSheet("background: transparent;");
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(4);
    lay->addWidget(editBtn);
    lay->addWidget(delBtn);
    return w;
}

static QWidget* makeLaunchCell(QPushButton* demoBtn, QPushButton* liveBtn, QWidget* parent) {
    auto* w = new QWidget(parent);
    w->setStyleSheet("background: transparent;");
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(4, 3, 4, 3);
    lay->setSpacing(6);
    demoBtn->setMinimumSize(78, 32);
    liveBtn->setMinimumSize(78, 32);
    lay->addWidget(demoBtn);
    lay->addWidget(liveBtn);
    return w;
}

static void setCellButtonsEnabled(QTableWidget* table, int row, int col, bool enabled) {
    QWidget* cell = table->cellWidget(row, col);
    if (!cell) return;
    if (auto* button = qobject_cast<QPushButton*>(cell)) { button->setEnabled(enabled); return; }
    for (auto* button : cell->findChildren<QPushButton*>())
        button->setEnabled(enabled);
}

static const QString TABLE_STYLE = QStringLiteral(R"QSS(
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
)QSS");

// ---------- construction -----------------------------------------------------

ConfigureModeScreen::ConfigureModeScreen(const QString& packageRoot, QWidget* parent)
    : QWidget(parent)
    , m_packageRoot(packageRoot)
    , m_workspaceRoot(WorkspacePaths::findWorkspaceRoot(packageRoot))
    , m_appManager(new AppManager(packageRoot, this))
    , m_adb(new AdbManager(this))
    , m_androidManager(new AndroidManager(m_adb, this))
    , m_mediaManager(new MediaManager(packageRoot, this))
{
    setFocusPolicy(Qt::StrongFocus);

    connect(m_appManager,      &AppManager::logMessage,     &Logger::instance(), &Logger::log);
    connect(m_mediaManager,    &MediaManager::logMessage,   &Logger::instance(), &Logger::log);
    connect(m_androidManager,  &AndroidManager::logMessage, &Logger::instance(), &Logger::log);
    connect(m_adb,             &AdbManager::log,            &Logger::instance(), &Logger::log);

    connect(m_appManager,     &AppManager::stateChanged,     this, &ConfigureModeScreen::onAppStateChanged);
    connect(m_androidManager, &AndroidManager::stateChanged, this, &ConfigureModeScreen::onAndroidStateChanged);
    connect(m_mediaManager,   &MediaManager::stateChanged,   this, &ConfigureModeScreen::onMediaStateChanged);
    connect(&Logger::instance(), &Logger::messageLogged,     this, &ConfigureModeScreen::onLogMessage);

    connect(m_adb, &AdbManager::deviceFound, this, [this](const QString& serial) {
        if (m_adbStatusLabel)
            m_adbStatusLabel->setText(QString("● Conectado: %1").arg(serial));
    });
    connect(m_adb, &AdbManager::deviceLost, this, [this]() {
        if (m_adbStatusLabel)
            m_adbStatusLabel->setText("○ Sin dispositivo");
    });

    buildUI();
    loadAppsConfig();
    populateAppsTable();
    loadAndroidConfig();
    populateAndroidTable();
    loadMediaConfig();
    populateMediaTable();

    connect(qGuiApp, &QGuiApplication::screenAdded,   this, [this](QScreen*) { populateScreenCombo(); loadStageConfig(); updateStageStatus(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) { populateScreenCombo(); loadStageConfig(); updateStageStatus(); });
}

// ---------- UI ---------------------------------------------------------------

void ConfigureModeScreen::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(16, 14, 16, 14);

    // Header
    auto* header = new QHBoxLayout();
    auto* title = new QLabel("CONFIGURAR", this);
    title->setObjectName("ScreenTitle");
    header->addWidget(title);
    header->addStretch();
    auto* hint = new QLabel("2 · Ensayo   3 · Show   Esc · Selector", this);
    hint->setObjectName("MutedLabel");
    header->addWidget(hint);
    root->addLayout(header);

    // Stage row
    auto* stageRow = new QHBoxLayout();
    stageRow->setSpacing(8);
    auto* stageLabel = new QLabel("Escenario", this);
    stageLabel->setObjectName("FieldLabel");
    stageRow->addWidget(stageLabel);
    m_screenCombo = new QComboBox(this);
    m_screenCombo->setFocusPolicy(Qt::NoFocus);
    m_screenCombo->setMinimumWidth(260);
    stageRow->addWidget(m_screenCombo);
    m_stageActivateBtn = new QPushButton("Activar", this);
    m_stageActivateBtn->setFocusPolicy(Qt::NoFocus);
    m_stageActivateBtn->setMinimumWidth(90);
    connect(m_stageActivateBtn, &QPushButton::clicked, this, &ConfigureModeScreen::onActivateStage);
    stageRow->addWidget(m_stageActivateBtn);
    stageRow->addStretch();
    root->addLayout(stageRow);

    // ── Tabs ──────────────────────────────────────────────────────────────────
    auto* tabs = new QTabWidget(this);
    tabs->setFocusPolicy(Qt::NoFocus);
    root->addWidget(tabs, 1);

    // ── Tab: ADB ──────────────────────────────────────────────────────────────
    {
        auto* page = new QWidget(tabs);
        auto* lay  = new QVBoxLayout(page);
        lay->setContentsMargins(12, 12, 12, 12);
        lay->setSpacing(12);

        // Row 1: device status + Detectar + Desconectar
        auto* statusRow = new QHBoxLayout();
        statusRow->setSpacing(8);
        m_adbStatusLabel = new QLabel("○ Sin dispositivo", page);
        m_adbStatusLabel->setObjectName("StatusInfo");
        statusRow->addWidget(m_adbStatusLabel);
        auto* detectBtn = new QPushButton("Detectar", page);
        detectBtn->setFocusPolicy(Qt::NoFocus);
        connect(detectBtn, &QPushButton::clicked, this, [this]() { m_adb->detectDevice(); });
        statusRow->addWidget(detectBtn);
        auto* testBtn = new QPushButton("Probar", page);
        testBtn->setFocusPolicy(Qt::NoFocus);
        connect(testBtn, &QPushButton::clicked, this, [this]() { m_adb->testConnection(); });
        statusRow->addWidget(testBtn);
        auto* disconnectBtn = new QPushButton("Desconectar", page);
        disconnectBtn->setFocusPolicy(Qt::NoFocus);
        connect(disconnectBtn, &QPushButton::clicked, this, [this]() { m_adb->disconnectDevice(); });
        statusRow->addWidget(disconnectBtn);
        statusRow->addStretch();
        lay->addLayout(statusRow);

        // Row 2: WiFi connect (IP:port + Conectar)
        auto* wifiRow = new QHBoxLayout();
        wifiRow->setSpacing(8);
        auto* ipLabel = new QLabel("IP:Puerto WiFi", page);
        ipLabel->setObjectName("FieldLabel");
        wifiRow->addWidget(ipLabel);
        auto* ipEdit = new QLineEdit(page);
        ipEdit->setPlaceholderText("192.168.1.x:5555");
        ipEdit->setMinimumWidth(180);
        ipEdit->setMaximumWidth(260);
        wifiRow->addWidget(ipEdit);
        auto* connectBtn = new QPushButton("Conectar", page);
        connectBtn->setFocusPolicy(Qt::NoFocus);
        connect(connectBtn, &QPushButton::clicked, this, [this, ipEdit]() {
            const QString ip = ipEdit->text().trimmed();
            if (!ip.isEmpty()) m_adb->connectWifi(ip);
        });
        wifiRow->addWidget(connectBtn);
        wifiRow->addStretch();
        lay->addLayout(wifiRow);
        lay->addStretch();

        tabs->addTab(page, "ADB");
    }

    // ── Tab: Qt ───────────────────────────────────────────────────────────────
    {
        auto* page = new QWidget(tabs);
        auto* lay  = new QVBoxLayout(page);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->setSpacing(6);

        m_appsTable = new QTableWidget(0, 5, page);
        m_appsTable->setHorizontalHeaderLabels({"Aplicación", "Iniciar", "Parar", "Estado", ""});
        m_appsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_appsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); m_appsTable->setColumnWidth(1, 180);
        m_appsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); m_appsTable->setColumnWidth(2, 104);
        m_appsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); m_appsTable->setColumnWidth(3, 120);
        m_appsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed); m_appsTable->setColumnWidth(4, 68);
        m_appsTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_appsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_appsTable->setFocusPolicy(Qt::NoFocus);
        m_appsTable->verticalHeader()->setVisible(false);
        m_appsTable->setShowGrid(false);
        m_appsTable->setStyleSheet(TABLE_STYLE);
        lay->addWidget(m_appsTable, 1);

        auto* bar = new QHBoxLayout();
        auto* addBtn = new QPushButton("+ Añadir", page);
        addBtn->setFocusPolicy(Qt::NoFocus);
        addBtn->setMinimumWidth(100);
        connect(addBtn, &QPushButton::clicked, this, &ConfigureModeScreen::addApp);
        bar->addWidget(addBtn);
        bar->addStretch();
        lay->addLayout(bar);

        tabs->addTab(page, "Qt");
    }

    // ── Tab: Android ──────────────────────────────────────────────────────────
    {
        auto* page = new QWidget(tabs);
        auto* lay  = new QVBoxLayout(page);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->setSpacing(6);

        m_androidTable = new QTableWidget(0, 5, page);
        m_androidTable->setHorizontalHeaderLabels({"Aplicación", "Lanzar", "Parar", "Estado", ""});
        m_androidTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_androidTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); m_androidTable->setColumnWidth(1, 104);
        m_androidTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); m_androidTable->setColumnWidth(2, 104);
        m_androidTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); m_androidTable->setColumnWidth(3, 120);
        m_androidTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed); m_androidTable->setColumnWidth(4, 68);
        m_androidTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_androidTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_androidTable->setFocusPolicy(Qt::NoFocus);
        m_androidTable->verticalHeader()->setVisible(false);
        m_androidTable->setShowGrid(false);
        m_androidTable->setStyleSheet(TABLE_STYLE);
        lay->addWidget(m_androidTable, 1);

        auto* bar = new QHBoxLayout();
        auto* addBtn = new QPushButton("+ Añadir", page);
        addBtn->setFocusPolicy(Qt::NoFocus);
        addBtn->setMinimumWidth(100);
        connect(addBtn, &QPushButton::clicked, this, &ConfigureModeScreen::addAndroidApp);
        bar->addWidget(addBtn);
        bar->addStretch();
        lay->addLayout(bar);

        tabs->addTab(page, "Android");
    }

    // ── Tab: Multimedia ───────────────────────────────────────────────────────
    {
        auto* page = new QWidget(tabs);
        auto* lay  = new QVBoxLayout(page);
        lay->setContentsMargins(8, 8, 8, 8);
        lay->setSpacing(6);

        m_mediaTable = new QTableWidget(0, 6, page);
        m_mediaTable->setHorizontalHeaderLabels({"Tipo", "Nombre", "Iniciar", "Parar", "Estado", ""});
        m_mediaTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed); m_mediaTable->setColumnWidth(0, 76);
        m_mediaTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_mediaTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); m_mediaTable->setColumnWidth(2, 104);
        m_mediaTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); m_mediaTable->setColumnWidth(3, 104);
        m_mediaTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed); m_mediaTable->setColumnWidth(4, 120);
        m_mediaTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed); m_mediaTable->setColumnWidth(5, 68);
        m_mediaTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_mediaTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_mediaTable->setFocusPolicy(Qt::NoFocus);
        m_mediaTable->verticalHeader()->setVisible(false);
        m_mediaTable->setShowGrid(false);
        m_mediaTable->setStyleSheet(TABLE_STYLE);
        lay->addWidget(m_mediaTable, 1);

        auto* bar = new QHBoxLayout();
        auto* addBtn = new QPushButton("+ Añadir multimedia", page);
        addBtn->setFocusPolicy(Qt::NoFocus);
        addBtn->setMinimumWidth(140);
        connect(addBtn, &QPushButton::clicked, this, &ConfigureModeScreen::addMedia);
        bar->addWidget(addBtn);
        bar->addStretch();
        lay->addLayout(bar);

        tabs->addTab(page, "Multimedia");
    }

    // Log panel
    m_logPanel = new QTextEdit(this);
    m_logPanel->setReadOnly(true);
    m_logPanel->setStyleSheet(
        "QTextEdit {"
        "  background-color: #0A0E14;"
        "  border: 1px solid #293241;"
        "  border-radius: 8px;"
        "  color: #00FF55;"
        "  font-family: 'Consolas', 'JetBrains Mono', monospace;"
        "  font-size: 11px;"
        "  padding: 8px;"
        "}"
    );
    m_logPanel->setFixedHeight(140);
    m_logPanel->setVisible(false);
    root->addWidget(m_logPanel);
}

// ---------- Qt apps: load / populate / update --------------------------------

void ConfigureModeScreen::loadAppsConfig() {
    m_appsConfigPath = QDir(m_packageRoot).filePath("config/apps.json");
    if (!QFileInfo::exists(m_appsConfigPath)) {
        Logger::instance().log("config/apps.json not found — creating default.");
        if (!AppConfig::copyDefaultTo(m_appsConfigPath))
            Logger::instance().log("WARNING: Could not write default apps config.");
    }
    if (!m_appsConfig.loadFromFile(m_appsConfigPath)) {
        Logger::instance().log("ERROR: Failed to parse config/apps.json.");
        return;
    }
    m_appManager->loadApps(m_appsConfig.apps());
    Logger::instance().log(QString("Apps Qt cargadas: %1.").arg(m_appsConfig.apps().size()));
}

void ConfigureModeScreen::populateAppsTable() {
    m_appsTable->setRowCount(0);
    for (int row = 0; row < m_appManager->entries().size(); ++row) {
        const AppEntry& e = m_appManager->entries()[row];
        m_appsTable->insertRow(row);

        auto* nameItem = new QTableWidgetItem(e.name);
        nameItem->setForeground(CyberTheme::color(CyberTheme::TextPrimary));
        m_appsTable->setItem(row, 0, nameItem);

        auto* demoBtn = new QPushButton("Demo", this);
        auto* liveBtn = new QPushButton("Live", this);
        auto* stopBtn = new QPushButton("Parar", this);
        demoBtn->setFocusPolicy(Qt::NoFocus);
        liveBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setEnabled(false);

        const QString id = e.id;
        connect(demoBtn, &QPushButton::clicked, this, [this, id]() { m_appManager->start(id, AppLaunchMode::Demo); });
        connect(liveBtn, &QPushButton::clicked, this, [this, id]() { m_appManager->start(id, AppLaunchMode::Live); });
        connect(stopBtn, &QPushButton::clicked, this, [this, id]() { m_appManager->stop(id); });
        m_appsTable->setCellWidget(row, 1, makeLaunchCell(demoBtn, liveBtn, this));
        m_appsTable->setCellWidget(row, 2, stopBtn);

        auto* stateItem = new QTableWidgetItem(appStateLabel(AppState::Stopped));
        stateItem->setForeground(appStateColor(AppState::Stopped));
        m_appsTable->setItem(row, 3, stateItem);

        auto* editBtn = makeIconButton(QStyle::SP_FileDialogDetailedView, "Cambiar ejecutable", this);
        auto* delBtn  = makeDangerIconButton(QStyle::SP_TrashIcon, "Eliminar", this);
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { editApp(id); });
        connect(delBtn,  &QPushButton::clicked, this, [this, id]() { deleteApp(id); });
        m_appsTable->setCellWidget(row, 4, makeActionCell(editBtn, delBtn, this));

        m_appsTable->setRowHeight(row, 44);
    }
}

int ConfigureModeScreen::appRowForId(const QString& id) const {
    const auto& entries = m_appManager->entries();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].id == id) return i;
    return -1;
}

int ConfigureModeScreen::appConfigIndexForId(const QString& id) const {
    const auto& apps = m_appsConfig.apps();
    for (int i = 0; i < apps.size(); ++i)
        if (apps[i].id == id) return i;
    return -1;
}

void ConfigureModeScreen::updateAppRow(const QString& id) {
    int row = appRowForId(id);
    if (row < 0) return;
    AppState s = m_appManager->state(id);
    if (auto* item = m_appsTable->item(row, 3)) {
        item->setText(appStateLabel(s));
        item->setForeground(appStateColor(s));
    }
    bool canStart = (s == AppState::Stopped || s == AppState::Error);
    bool canStop  = (s == AppState::Starting || s == AppState::Running || s == AppState::Stopping);
    setCellButtonsEnabled(m_appsTable, row, 1, canStart);
    if (auto* b = qobject_cast<QPushButton*>(m_appsTable->cellWidget(row, 2))) b->setEnabled(canStop);
}

// ---------- Qt apps: CRUD ----------------------------------------------------

void ConfigureModeScreen::applyAppsChanges() {
    if (!m_appsConfig.saveToFile(m_appsConfigPath))
        Logger::instance().log("ERROR: Could not save config/apps.json.");
    m_appManager->loadApps(m_appsConfig.apps());
    populateAppsTable();
}

void ConfigureModeScreen::addApp() {
    const QString startDir = WorkspacePaths::preferredWorkspaceChildDir(m_workspaceRoot, "dist_qt");
    QString exePath = QFileDialog::getOpenFileName(
        this, "Seleccionar ejecutable", startDir, "Executables (*.exe)");
    if (exePath.isEmpty()) return;

    QFileInfo fi(exePath);
    AppEntry e;
    e.id               = fi.completeBaseName();
    e.name             = fi.completeBaseName();
    e.executable       = fi.fileName();
    e.workingDirectory = WorkspacePaths::relativizeToWorkspace(m_workspaceRoot, fi.absolutePath());

    if (QMessageBox::question(this, "Añadir aplicación Qt",
            QString("¿Añadir '%1'?\n\nEjecutable:  %2\nDirectorio: %3")
                .arg(e.name, e.executable, e.workingDirectory))
        != QMessageBox::Yes) return;

    m_appsConfig.addApp(e);
    applyAppsChanges();
    Logger::instance().log(QString("App Qt añadida: %1").arg(e.name));
}

void ConfigureModeScreen::editApp(const QString& id) {
    int ci = appConfigIndexForId(id);
    if (ci < 0) return;

    if (m_appManager->state(id) != AppState::Stopped && m_appManager->state(id) != AppState::Error) {
        QMessageBox::warning(this, "App en ejecución",
            QString("Para '%1' antes de modificarla.").arg(m_appsConfig.apps()[ci].name));
        return;
    }

    const AppEntry& current = m_appsConfig.apps()[ci];
    const QString currentWorkingDir = WorkspacePaths::absoluteFromWorkspace(m_workspaceRoot, current.workingDirectory);
    QString startDir = QFileInfo::exists(currentWorkingDir)
        ? currentWorkingDir : m_packageRoot;
    QString exePath = QFileDialog::getOpenFileName(
        this, "Seleccionar ejecutable", startDir, "Executables (*.exe)");
    if (exePath.isEmpty()) return;

    QFileInfo fi(exePath);
    AppEntry updated         = current;
    updated.executable       = fi.fileName();
    updated.workingDirectory = WorkspacePaths::relativizeToWorkspace(m_workspaceRoot, fi.absolutePath());

    if (QMessageBox::question(this, "Actualizar aplicación Qt",
            QString("¿Actualizar '%1'?\n\nEjecutable:  %2\nDirectorio: %3")
                .arg(updated.name, updated.executable, updated.workingDirectory))
        != QMessageBox::Yes) return;

    m_appsConfig.updateApp(ci, updated);
    applyAppsChanges();
    Logger::instance().log(QString("App Qt actualizada: %1").arg(updated.name));
}

void ConfigureModeScreen::deleteApp(const QString& id) {
    int ci = appConfigIndexForId(id);
    if (ci < 0) return;

    if (m_appManager->state(id) != AppState::Stopped && m_appManager->state(id) != AppState::Error) {
        QMessageBox::warning(this, "App en ejecución",
            QString("Para '%1' antes de eliminarla.").arg(m_appsConfig.apps()[ci].name));
        return;
    }

    QString name = m_appsConfig.apps()[ci].name;
    if (QMessageBox::question(this, "Eliminar aplicación Qt",
            QString("¿Eliminar '%1'?").arg(name))
        != QMessageBox::Yes) return;

    m_appsConfig.removeApp(ci);
    applyAppsChanges();
    Logger::instance().log(QString("App Qt eliminada: %1").arg(name));
}

// ---------- Android: load / populate / update --------------------------------

void ConfigureModeScreen::loadAndroidConfig() {
    m_androidConfigPath = QDir(m_packageRoot).filePath("config/android.json");
    if (!QFileInfo::exists(m_androidConfigPath)) {
        Logger::instance().log("config/android.json not found — creating default.");
        if (!AndroidConfig::copyDefaultTo(m_androidConfigPath))
            Logger::instance().log("WARNING: Could not write default android config.");
    }
    if (!m_androidConfig.loadFromFile(m_androidConfigPath)) {
        Logger::instance().log("ERROR: Failed to parse config/android.json.");
        return;
    }
    m_androidManager->loadApps(m_androidConfig.apps());
    Logger::instance().log(QString("Apps Android cargadas: %1.").arg(m_androidConfig.apps().size()));
}

void ConfigureModeScreen::populateAndroidTable() {
    m_androidTable->setRowCount(0);
    for (int row = 0; row < m_androidManager->entries().size(); ++row) {
        const AndroidEntry& e = m_androidManager->entries()[row];
        m_androidTable->insertRow(row);

        auto* nameItem = new QTableWidgetItem(e.name);
        nameItem->setForeground(CyberTheme::color(CyberTheme::TextPrimary));
        nameItem->setToolTip(e.package);
        m_androidTable->setItem(row, 0, nameItem);

        auto* launchBtn = new QPushButton("Lanzar", this);
        auto* stopBtn   = new QPushButton("Parar",  this);
        launchBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setEnabled(false);

        const QString id = e.id;
        connect(launchBtn, &QPushButton::clicked, this, [this, id]() { m_androidManager->start(id); });
        connect(stopBtn,   &QPushButton::clicked, this, [this, id]() { m_androidManager->stop(id); });
        m_androidTable->setCellWidget(row, 1, launchBtn);
        m_androidTable->setCellWidget(row, 2, stopBtn);

        auto* stateItem = new QTableWidgetItem(androidStateLabel(AndroidState::Stopped));
        stateItem->setForeground(androidStateColor(AndroidState::Stopped));
        m_androidTable->setItem(row, 3, stateItem);

        auto* editBtn = makeIconButton(QStyle::SP_FileDialogDetailedView, "Editar", this);
        auto* delBtn  = makeDangerIconButton(QStyle::SP_TrashIcon, "Eliminar", this);
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { editAndroidApp(id); });
        connect(delBtn,  &QPushButton::clicked, this, [this, id]() { deleteAndroidApp(id); });
        m_androidTable->setCellWidget(row, 4, makeActionCell(editBtn, delBtn, this));

        m_androidTable->setRowHeight(row, 44);
    }
}

int ConfigureModeScreen::androidRowForId(const QString& id) const {
    const auto& entries = m_androidManager->entries();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].id == id) return i;
    return -1;
}

int ConfigureModeScreen::androidConfigIndexForId(const QString& id) const {
    const auto& apps = m_androidConfig.apps();
    for (int i = 0; i < apps.size(); ++i)
        if (apps[i].id == id) return i;
    return -1;
}

void ConfigureModeScreen::updateAndroidRow(const QString& id) {
    int row = androidRowForId(id);
    if (row < 0) return;
    AndroidState s = m_androidManager->state(id);
    if (auto* item = m_androidTable->item(row, 3)) {
        item->setText(androidStateLabel(s));
        item->setForeground(androidStateColor(s));
    }
    bool running = (s == AndroidState::Running);
    if (auto* b = qobject_cast<QPushButton*>(m_androidTable->cellWidget(row, 1))) b->setEnabled(!running);
    if (auto* b = qobject_cast<QPushButton*>(m_androidTable->cellWidget(row, 2))) b->setEnabled(running);
}

// ---------- Android: CRUD ----------------------------------------------------

bool ConfigureModeScreen::showAndroidDialog(AndroidEntry& entry, const QString& title) {
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(title);
    dlg->setMinimumWidth(400);

    auto* form = new QFormLayout(dlg);
    form->setContentsMargins(16, 16, 16, 8);
    form->setSpacing(10);

    auto* nameEdit     = new QLineEdit(entry.name,     dlg);
    auto* packageEdit  = new QLineEdit(entry.package,  dlg);
    auto* activityEdit = new QLineEdit(entry.activity, dlg);
    auto* portSpin     = new QSpinBox(dlg);
    portSpin->setRange(0, 65535);
    portSpin->setValue(entry.wsPort);
    portSpin->setSpecialValueText("Sin túnel");

    form->addRow("Nombre:",    nameEdit);
    form->addRow("Paquete:",   packageEdit);
    form->addRow("Actividad:", activityEdit);
    form->addRow("Puerto WS:", portSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    if (dlg->exec() != QDialog::Accepted) return false;

    entry.name     = nameEdit->text().trimmed();
    entry.package  = packageEdit->text().trimmed();
    entry.activity = activityEdit->text().trimmed();
    if (entry.activity.isEmpty()) entry.activity = ".MainActivity";
    entry.wsPort   = static_cast<quint16>(portSpin->value());
    return !entry.name.isEmpty() && !entry.package.isEmpty();
}

void ConfigureModeScreen::applyAndroidChanges() {
    if (!m_androidConfig.saveToFile(m_androidConfigPath))
        Logger::instance().log("ERROR: Could not save config/android.json.");
    m_androidManager->loadApps(m_androidConfig.apps());
    populateAndroidTable();
}

void ConfigureModeScreen::addAndroidApp() {
    AndroidEntry e;
    e.activity = ".MainActivity";
    if (!showAndroidDialog(e, "Añadir app Android")) return;
    e.id = e.package;   // use package as stable id
    m_androidConfig.addApp(e);
    applyAndroidChanges();
    Logger::instance().log(QString("App Android añadida: %1").arg(e.name));
}

void ConfigureModeScreen::editAndroidApp(const QString& id) {
    int ci = androidConfigIndexForId(id);
    if (ci < 0) return;

    AndroidEntry updated = m_androidConfig.apps()[ci];
    if (!showAndroidDialog(updated, "Editar app Android")) return;

    m_androidConfig.updateApp(ci, updated);
    applyAndroidChanges();
    Logger::instance().log(QString("App Android actualizada: %1").arg(updated.name));
}

void ConfigureModeScreen::deleteAndroidApp(const QString& id) {
    int ci = androidConfigIndexForId(id);
    if (ci < 0) return;

    QString name = m_androidConfig.apps()[ci].name;
    if (QMessageBox::question(this, "Eliminar app Android",
            QString("¿Eliminar '%1'?").arg(name))
        != QMessageBox::Yes) return;

    m_androidConfig.removeApp(ci);
    applyAndroidChanges();
    Logger::instance().log(QString("App Android eliminada: %1").arg(name));
}

// ---------- Media: load / populate / update ----------------------------------

void ConfigureModeScreen::loadMediaConfig() {
    m_mediaConfigPath = QDir(m_packageRoot).filePath("config/media.json");
    if (!QFileInfo::exists(m_mediaConfigPath)) {
        Logger::instance().log("config/media.json not found — creating default.");
        if (!MediaConfig::copyDefaultTo(m_mediaConfigPath))
            Logger::instance().log("WARNING: Could not write default media config.");
    }
    if (!m_mediaConfig.loadFromFile(m_mediaConfigPath)) {
        Logger::instance().log("ERROR: Failed to parse config/media.json.");
        return;
    }
    m_mediaManager->loadMedia(m_mediaConfig.items());
    Logger::instance().log(QString("Media cargado: %1 archivo(s).").arg(m_mediaConfig.items().size()));
}

void ConfigureModeScreen::populateMediaTable() {
    m_mediaTable->setRowCount(0);
    for (int row = 0; row < m_mediaManager->entries().size(); ++row) {
        const MediaEntry& e = m_mediaManager->entries()[row];
        m_mediaTable->insertRow(row);

        bool isVideo = (e.type == "video");
        auto* typeItem = new QTableWidgetItem(isVideo ? "VIDEO" : "AUDIO");
        typeItem->setForeground(isVideo ? QColor("#00BFFF") : QColor("#FF8C00"));
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_mediaTable->setItem(row, 0, typeItem);

        auto* nameItem = new QTableWidgetItem(e.name);
        nameItem->setForeground(CyberTheme::color(CyberTheme::TextPrimary));
        m_mediaTable->setItem(row, 1, nameItem);

        auto* playBtn = new QPushButton("Iniciar", this);
        auto* stopBtn = new QPushButton("Parar",   this);
        playBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setEnabled(false);

        const QString id = e.id;
        connect(playBtn, &QPushButton::clicked, this, [this, id]() { m_mediaManager->play(id); });
        connect(stopBtn, &QPushButton::clicked, this, [this, id]() { m_mediaManager->stop(id); });
        m_mediaTable->setCellWidget(row, 2, playBtn);
        m_mediaTable->setCellWidget(row, 3, stopBtn);

        auto* stateItem = new QTableWidgetItem(mediaStateLabel(MediaState::Stopped));
        stateItem->setForeground(mediaStateColor(MediaState::Stopped));
        m_mediaTable->setItem(row, 4, stateItem);

        auto* editBtn = makeIconButton(QStyle::SP_FileDialogDetailedView, "Cambiar archivo", this);
        auto* delBtn  = makeDangerIconButton(QStyle::SP_TrashIcon, "Eliminar", this);
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { editMedia(id); });
        connect(delBtn,  &QPushButton::clicked, this, [this, id]() { deleteMedia(id); });
        m_mediaTable->setCellWidget(row, 5, makeActionCell(editBtn, delBtn, this));

        m_mediaTable->setRowHeight(row, 44);
    }
}

int ConfigureModeScreen::mediaRowForId(const QString& id) const {
    const auto& entries = m_mediaManager->entries();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].id == id) return i;
    return -1;
}

int ConfigureModeScreen::mediaConfigIndexForId(const QString& id) const {
    const auto& items = m_mediaConfig.items();
    for (int i = 0; i < items.size(); ++i)
        if (items[i].id == id) return i;
    return -1;
}

void ConfigureModeScreen::updateMediaRow(const QString& id) {
    int row = mediaRowForId(id);
    if (row < 0) return;
    MediaState s = m_mediaManager->state(id);
    if (auto* item = m_mediaTable->item(row, 4)) {
        item->setText(mediaStateLabel(s));
        item->setForeground(mediaStateColor(s));
    }
    bool canPlay = (s == MediaState::Stopped || s == MediaState::Error);
    bool canStop = (s == MediaState::Playing);
    if (auto* b = qobject_cast<QPushButton*>(m_mediaTable->cellWidget(row, 2))) b->setEnabled(canPlay);
    if (auto* b = qobject_cast<QPushButton*>(m_mediaTable->cellWidget(row, 3))) b->setEnabled(canStop);
}

// ---------- Media: CRUD ------------------------------------------------------

void ConfigureModeScreen::applyMediaChanges() {
    if (!m_mediaConfig.saveToFile(m_mediaConfigPath))
        Logger::instance().log("ERROR: Could not save config/media.json.");
    m_mediaManager->loadMedia(m_mediaConfig.items());
    populateMediaTable();
}

void ConfigureModeScreen::addMedia() {
    const QString startDir = WorkspacePaths::preferredWorkspaceChildDir(m_workspaceRoot, "dist_media");
    QString filePath = QFileDialog::getOpenFileName(
        this, "Seleccionar archivo multimedia", startDir,
        "Archivos multimedia (*.mp4 *.mov *.avi *.mkv *.webm *.mp3 *.wav *.ogg *.flac *.aac *.m4a)");
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    MediaEntry e;
    e.id   = fi.completeBaseName();
    e.name = fi.completeBaseName();
    e.type = mediaTypeForPath(filePath);
    e.path = WorkspacePaths::relativizeToWorkspace(m_workspaceRoot, fi.absoluteFilePath());

    if (QMessageBox::question(this, "Añadir multimedia",
            QString("¿Añadir '%1'?\n\nArchivo: %2\nTipo: %3").arg(e.name, e.path, e.type))
        != QMessageBox::Yes) return;

    m_mediaConfig.addItem(e);
    applyMediaChanges();
    Logger::instance().log(QString("Media añadido: %1 (%2)").arg(e.name, e.type));
}

void ConfigureModeScreen::editMedia(const QString& id) {
    int ci = mediaConfigIndexForId(id);
    if (ci < 0) return;

    if (m_mediaManager->state(id) != MediaState::Stopped && m_mediaManager->state(id) != MediaState::Error) {
        QMessageBox::warning(this, "Media en reproducción",
            QString("Para '%1' antes de modificarlo.").arg(m_mediaConfig.items()[ci].name));
        return;
    }

    const MediaEntry& current = m_mediaConfig.items()[ci];
    QString startDir = QFileInfo(WorkspacePaths::absoluteFromWorkspace(m_workspaceRoot, current.path)).absolutePath();
    if (!QFileInfo::exists(startDir)) startDir = WorkspacePaths::preferredWorkspaceChildDir(m_workspaceRoot, "dist_media");

    QString filePath = QFileDialog::getOpenFileName(
        this, "Seleccionar archivo multimedia", startDir,
        "Archivos multimedia (*.mp4 *.mov *.avi *.mkv *.webm *.mp3 *.wav *.ogg *.flac *.aac *.m4a)");
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    MediaEntry updated = current;
    updated.path = WorkspacePaths::relativizeToWorkspace(m_workspaceRoot, fi.absoluteFilePath());
    updated.type = mediaTypeForPath(filePath);

    if (QMessageBox::question(this, "Actualizar multimedia",
            QString("¿Actualizar '%1'?\n\nNuevo archivo: %2\nTipo: %3")
                .arg(updated.name, updated.path, updated.type))
        != QMessageBox::Yes) return;

    m_mediaConfig.updateItem(ci, updated);
    applyMediaChanges();
    Logger::instance().log(QString("Media actualizado: %1").arg(updated.name));
}

void ConfigureModeScreen::deleteMedia(const QString& id) {
    int ci = mediaConfigIndexForId(id);
    if (ci < 0) return;

    if (m_mediaManager->state(id) != MediaState::Stopped && m_mediaManager->state(id) != MediaState::Error) {
        QMessageBox::warning(this, "Media en reproducción",
            QString("Para '%1' antes de eliminarlo.").arg(m_mediaConfig.items()[ci].name));
        return;
    }

    QString name = m_mediaConfig.items()[ci].name;
    if (QMessageBox::question(this, "Eliminar multimedia",
            QString("¿Eliminar '%1'?").arg(name))
        != QMessageBox::Yes) return;

    m_mediaConfig.removeItem(ci);
    applyMediaChanges();
    Logger::instance().log(QString("Media eliminado: %1").arg(name));
}

// ---------- slots ------------------------------------------------------------

void ConfigureModeScreen::onAppStateChanged(const QString& id, AppState)         { updateAppRow(id); }
void ConfigureModeScreen::onAndroidStateChanged(const QString& id, AndroidState) { updateAndroidRow(id); }
void ConfigureModeScreen::onMediaStateChanged(const QString& id, MediaState)     { updateMediaRow(id); }

void ConfigureModeScreen::onLogMessage(const QString& formatted) {
    m_logPanel->append(formatted);
    m_logPanel->verticalScrollBar()->setValue(m_logPanel->verticalScrollBar()->maximum());
}

// ---------- stage ------------------------------------------------------------

void ConfigureModeScreen::setStageWindow(StageWindow* stage) {
    m_stageWindow = stage;
    if (!stage) return;
    connect(stage, &StageWindow::activated,   this, &ConfigureModeScreen::updateStageStatus);
    connect(stage, &StageWindow::deactivated, this, &ConfigureModeScreen::updateStageStatus);
    loadStageConfig();
    updateStageStatus();
}

void ConfigureModeScreen::populateScreenCombo() {
    const int saved = m_screenCombo->currentIndex();
    m_screenCombo->clear();
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        const QRect geo = screens[i]->geometry();
        m_screenCombo->addItem(QString("Pantalla %1: %2 (%3×%4)")
            .arg(i + 1).arg(screens[i]->name()).arg(geo.width()).arg(geo.height()));
    }
    const int target = (saved >= 0 && saved < m_screenCombo->count())
        ? saved : m_screenCombo->count() - 1;
    if (target >= 0) m_screenCombo->setCurrentIndex(target);

    const bool multi = screens.size() > 1;
    m_screenCombo->setVisible(multi);
    m_stageActivateBtn->setVisible(multi);
}

void ConfigureModeScreen::onActivateStage() {
    if (!m_stageWindow) return;
    if (m_stageWindow->isActive()) {
        m_stageWindow->deactivate();
    } else {
        const int idx = m_screenCombo->currentIndex();
        m_stageWindow->activateOnScreen(idx);
        saveStageConfig(idx);
    }
}

void ConfigureModeScreen::updateStageStatus() {
    if (QGuiApplication::screens().size() <= 1) return;
    const bool active = m_stageWindow && m_stageWindow->isActive();
    m_stageActivateBtn->setText(active ? "Desactivar" : "Activar");
    m_screenCombo->setEnabled(!active);
    if (active) m_stageWindow->showLogo();
}

void ConfigureModeScreen::loadStageConfig() {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
    if (!QFileInfo::exists(path))
        DefaultConfigUtils::copyResourceDefaultTo(":/defaults/resources/stage.json", path);
    QFile f(path);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return;
    const int idx = doc.object()["screenIndex"].toInt(0);
    if (idx >= 0 && idx < m_screenCombo->count())
        m_screenCombo->setCurrentIndex(idx);
}

void ConfigureModeScreen::saveStageConfig(int screenIndex) {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return;
    QJsonObject obj;
    obj["screenIndex"] = screenIndex;
    f.write(QJsonDocument(obj).toJson());
}

// ---------- events -----------------------------------------------------------

void ConfigureModeScreen::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_F10:    m_logPanel->setVisible(!m_logPanel->isVisible()); break;
        case Qt::Key_Escape: emit returnToSelector(); break;
        case Qt::Key_2: case Qt::Key_Right: emit switchMode(1); break;
        case Qt::Key_3:                     emit switchMode(2); break;
        default: QWidget::keyPressEvent(event);
    }
}

void ConfigureModeScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setFocus();
    populateScreenCombo();
    loadStageConfig();
    updateStageStatus();
    m_adb->detectDevice();
}
