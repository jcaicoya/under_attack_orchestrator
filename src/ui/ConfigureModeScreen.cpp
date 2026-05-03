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
#include <QGuiApplication>
#include <QScreen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

// ---------- helpers ----------------------------------------------------------

static QString appStateLabel(AppState s) {
    switch (s) {
        case AppState::Stopped:  return "LISTA";
        case AppState::Starting: return "EJECUTÁNDOSE";
        case AppState::Running:  return "EJECUTÁNDOSE";
        case AppState::Stopping: return "EJECUTÁNDOSE";
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

static QLabel* makeSectionLabel(const QString& text, QWidget* parent) {
    auto* lbl = new QLabel(text, parent);
    lbl->setObjectName("FieldLabel");
    return lbl;
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
    , m_manager(new AppManager(packageRoot, this))
    , m_mediaManager(new MediaManager(this))
{
    setFocusPolicy(Qt::StrongFocus);

    connect(m_manager,      &AppManager::logMessage,   &Logger::instance(), &Logger::log);
    connect(m_mediaManager, &MediaManager::logMessage, &Logger::instance(), &Logger::log);
    connect(&Logger::instance(), &Logger::messageLogged, this, &ConfigureModeScreen::onLogMessage);
    connect(m_manager,      &AppManager::stateChanged,   this, &ConfigureModeScreen::onStateChanged);
    connect(m_mediaManager, &MediaManager::stateChanged, this, &ConfigureModeScreen::onMediaStateChanged);

    buildUI();
    loadConfig();
    populateTable();
    loadMediaConfig();
    populateMediaTable();
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
    auto* escHint = new QLabel("2 · Ensayo   3 · Show   →  Cambiar modo   Esc · Selector", this);
    escHint->setObjectName("MutedLabel");
    header->addWidget(escHint);
    root->addLayout(header);

    // ── Escenario ─────────────────────────────────────────────────────────────
    auto* stageRow = new QHBoxLayout();
    stageRow->setSpacing(8);
    auto* stageHeadLabel = new QLabel("Escenario", this);
    stageHeadLabel->setObjectName("FieldLabel");
    stageRow->addWidget(stageHeadLabel);

    m_screenCombo = new QComboBox(this);
    m_screenCombo->setFocusPolicy(Qt::NoFocus);
    m_screenCombo->setMinimumWidth(260);
    stageRow->addWidget(m_screenCombo);

    m_stageActivateBtn = new QPushButton("Activar", this);
    m_stageActivateBtn->setFocusPolicy(Qt::NoFocus);
    m_stageActivateBtn->setMinimumWidth(90);
    connect(m_stageActivateBtn, &QPushButton::clicked,
            this, &ConfigureModeScreen::onActivateStage);
    stageRow->addWidget(m_stageActivateBtn);

    stageRow->addSpacing(12);
    m_stageStatusLabel = new QLabel("Inactivo", this);
    m_stageStatusLabel->setObjectName("MutedLabel");
    stageRow->addWidget(m_stageStatusLabel);
    stageRow->addStretch();
    root->addLayout(stageRow);

    // ── Aplicaciones ─────────────────────────────────────────────────────────
    root->addWidget(makeSectionLabel("Aplicaciones", this));

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({"Aplicación", "Iniciar", "Parar", "Estado", ""});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); m_table->setColumnWidth(1, 90);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); m_table->setColumnWidth(2, 90);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); m_table->setColumnWidth(3, 120);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed); m_table->setColumnWidth(4, 68);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setStyleSheet(TABLE_STYLE);
    root->addWidget(m_table, 2);

    auto* appBar = new QHBoxLayout();
    m_addBtn = new QPushButton("+ Añadir", this);
    m_addBtn->setFocusPolicy(Qt::NoFocus);
    m_addBtn->setMinimumWidth(100);
    connect(m_addBtn, &QPushButton::clicked, this, &ConfigureModeScreen::addApp);
    appBar->addWidget(m_addBtn);
    appBar->addStretch();
    m_stopAllBtn = new QPushButton("Parar todo", this);
    m_stopAllBtn->setObjectName("DangerButton");
    m_stopAllBtn->setMinimumWidth(110);
    m_stopAllBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_stopAllBtn, &QPushButton::clicked, m_manager, &AppManager::stopAll);
    appBar->addWidget(m_stopAllBtn);
    root->addLayout(appBar);

    // ── Multimedia ───────────────────────────────────────────────────────────
    root->addWidget(makeSectionLabel("Multimedia", this));

    m_mediaTable = new QTableWidget(0, 6, this);
    m_mediaTable->setHorizontalHeaderLabels({"Nombre", "Tipo", "Reproducir", "Parar", "Estado", ""});
    m_mediaTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_mediaTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); m_mediaTable->setColumnWidth(1, 60);
    m_mediaTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); m_mediaTable->setColumnWidth(2, 90);
    m_mediaTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); m_mediaTable->setColumnWidth(3, 90);
    m_mediaTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed); m_mediaTable->setColumnWidth(4, 120);
    m_mediaTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed); m_mediaTable->setColumnWidth(5, 68);
    m_mediaTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_mediaTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mediaTable->setFocusPolicy(Qt::NoFocus);
    m_mediaTable->verticalHeader()->setVisible(false);
    m_mediaTable->setShowGrid(false);
    m_mediaTable->setStyleSheet(TABLE_STYLE);
    root->addWidget(m_mediaTable, 2);

    auto* mediaBar = new QHBoxLayout();
    m_addMediaBtn = new QPushButton("+ Añadir multimedia", this);
    m_addMediaBtn->setFocusPolicy(Qt::NoFocus);
    m_addMediaBtn->setMinimumWidth(140);
    connect(m_addMediaBtn, &QPushButton::clicked, this, &ConfigureModeScreen::addMedia);
    mediaBar->addWidget(m_addMediaBtn);
    mediaBar->addStretch();
    m_stopAllMediaBtn = new QPushButton("Parar todo", this);
    m_stopAllMediaBtn->setObjectName("DangerButton");
    m_stopAllMediaBtn->setMinimumWidth(110);
    m_stopAllMediaBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_stopAllMediaBtn, &QPushButton::clicked, m_mediaManager, &MediaManager::stopAll);
    mediaBar->addWidget(m_stopAllMediaBtn);
    root->addLayout(mediaBar);

    // ── Log ──────────────────────────────────────────────────────────────────
    root->addWidget(makeSectionLabel("Log:", this));

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
    root->addWidget(m_logPanel, 1);
}

// ---------- apps: load / populate / update -----------------------------------

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

    m_manager->loadApps(m_config.apps());
    Logger::instance().log(QString("Apps cargadas: %1.").arg(m_config.apps().size()));
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
        connect(stopBtn,  &QPushButton::clicked, this, [this, id]() { m_manager->stop(id); });
        m_table->setCellWidget(row, 1, startBtn);
        m_table->setCellWidget(row, 2, stopBtn);

        auto* stateItem = new QTableWidgetItem(appStateLabel(AppState::Stopped));
        stateItem->setForeground(appStateColor(AppState::Stopped));
        m_table->setItem(row, 3, stateItem);

        auto* editBtn = makeIconButton(QStyle::SP_FileDialogDetailedView, "Cambiar ejecutable", this);
        auto* delBtn  = makeDangerIconButton(QStyle::SP_TrashIcon, "Eliminar", this);
        connect(editBtn, &QPushButton::clicked, this, [this, id]() { editApp(id); });
        connect(delBtn,  &QPushButton::clicked, this, [this, id]() { deleteApp(id); });
        m_table->setCellWidget(row, 4, makeActionCell(editBtn, delBtn, this));

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

void ConfigureModeScreen::updateRow(const QString& id) {
    int row = rowForId(id);
    if (row < 0) return;
    AppState s = m_manager->state(id);
    if (auto* item = m_table->item(row, 3)) {
        item->setText(appStateLabel(s));
        item->setForeground(appStateColor(s));
    }
    bool canStart = (s == AppState::Stopped || s == AppState::Error);
    bool canStop  = (s == AppState::Starting || s == AppState::Running || s == AppState::Stopping);
    if (auto* b = qobject_cast<QPushButton*>(m_table->cellWidget(row, 1))) b->setEnabled(canStart);
    if (auto* b = qobject_cast<QPushButton*>(m_table->cellWidget(row, 2))) b->setEnabled(canStop);
}

// ---------- apps: CRUD -------------------------------------------------------

void ConfigureModeScreen::applyConfigChanges() {
    if (!m_config.saveToFile(m_configPath))
        Logger::instance().log("ERROR: Could not save config/apps.json.");
    m_manager->loadApps(m_config.apps());
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

    if (QMessageBox::question(this, "Añadir aplicación",
            QString("¿Añadir '%1'?\n\nEjecutable:  %2\nDirectorio: %3")
                .arg(e.name, e.executable, e.workingDirectory))
        != QMessageBox::Yes) return;

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

    if (QMessageBox::question(this, "Actualizar aplicación",
            QString("¿Actualizar '%1'?\n\nEjecutable:  %2\nDirectorio: %3")
                .arg(updated.name, updated.executable, updated.workingDirectory))
        != QMessageBox::Yes) return;

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

    QString name = m_config.apps()[ci].name;
    if (QMessageBox::question(this, "Eliminar aplicación",
            QString("¿Eliminar '%1'?").arg(name))
        != QMessageBox::Yes) return;

    m_config.removeApp(ci);
    applyConfigChanges();
    Logger::instance().log(QString("App eliminada: %1").arg(name));
}

// ---------- media: load / populate / update ----------------------------------

void ConfigureModeScreen::loadMediaConfig() {
    m_mediaConfigPath = QDir(m_packageRoot).filePath("config/media.json");

    if (!m_mediaConfig.loadFromFile(m_mediaConfigPath)) {
        Logger::instance().log("ERROR: Failed to parse config/media.json.");
        return;
    }

    m_mediaManager->loadMedia(m_mediaConfig.items());
    Logger::instance().log(QString("Media cargado: %1 archivo(s).").arg(m_mediaConfig.items().size()));
}

void ConfigureModeScreen::populateMediaTable() {
    m_mediaTable->setRowCount(0);
    const auto& entries = m_mediaManager->entries();

    for (int row = 0; row < entries.size(); ++row) {
        const MediaEntry& e = entries[row];
        m_mediaTable->insertRow(row);

        auto* nameItem = new QTableWidgetItem(e.name);
        nameItem->setForeground(CyberTheme::color(CyberTheme::TextPrimary));
        m_mediaTable->setItem(row, 0, nameItem);

        bool isVideo = (e.type == "video");
        auto* typeItem = new QTableWidgetItem(isVideo ? "VIDEO" : "AUDIO");
        typeItem->setForeground(isVideo ? QColor("#00BFFF") : QColor("#FF8C00"));
        m_mediaTable->setItem(row, 1, typeItem);

        auto* playBtn = new QPushButton("Reproducir", this);
        auto* stopBtn = new QPushButton("Parar",      this);
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

        m_mediaTable->setRowHeight(row, 38);
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

// ---------- media: CRUD ------------------------------------------------------

void ConfigureModeScreen::applyMediaChanges() {
    if (!m_mediaConfig.saveToFile(m_mediaConfigPath))
        Logger::instance().log("ERROR: Could not save config/media.json.");
    m_mediaManager->loadMedia(m_mediaConfig.items());
    populateMediaTable();
}

void ConfigureModeScreen::addMedia() {
    QString filePath = QFileDialog::getOpenFileName(
        this, "Seleccionar archivo multimedia", m_packageRoot,
        "Archivos multimedia (*.mp4 *.mov *.avi *.mkv *.webm *.mp3 *.wav *.ogg *.flac *.aac *.m4a)");
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    MediaEntry e;
    e.id      = fi.completeBaseName();
    e.name    = fi.completeBaseName();
    e.type    = mediaTypeForPath(filePath);
    e.path    = fi.absoluteFilePath();
    e.enabled = true;

    if (QMessageBox::question(this, "Añadir multimedia",
            QString("¿Añadir '%1'?\n\nArchivo: %2\nTipo: %3")
                .arg(e.name, e.path, e.type))
        != QMessageBox::Yes) return;

    m_mediaConfig.addItem(e);
    applyMediaChanges();
    Logger::instance().log(QString("Media añadido: %1 (%2)").arg(e.name, e.type));
}

void ConfigureModeScreen::editMedia(const QString& id) {
    int ci = mediaConfigIndexForId(id);
    if (ci < 0) return;

    if (m_mediaManager->state(id) != MediaState::Stopped &&
        m_mediaManager->state(id) != MediaState::Error) {
        QMessageBox::warning(this, "Media en reproducción",
            QString("Para '%1' antes de modificarlo.").arg(m_mediaConfig.items()[ci].name));
        return;
    }

    const MediaEntry& current = m_mediaConfig.items()[ci];
    QString startDir = QFileInfo(current.path).absolutePath();
    if (!QFileInfo::exists(startDir)) startDir = m_packageRoot;

    QString filePath = QFileDialog::getOpenFileName(
        this, "Seleccionar archivo multimedia", startDir,
        "Archivos multimedia (*.mp4 *.mov *.avi *.mkv *.webm *.mp3 *.wav *.ogg *.flac *.aac *.m4a)");
    if (filePath.isEmpty()) return;

    QFileInfo fi(filePath);
    MediaEntry updated = current;
    updated.path = fi.absoluteFilePath();
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

    if (m_mediaManager->state(id) != MediaState::Stopped &&
        m_mediaManager->state(id) != MediaState::Error) {
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

void ConfigureModeScreen::onStateChanged(const QString& id, AppState) { updateRow(id); }
void ConfigureModeScreen::onMediaStateChanged(const QString& id, MediaState) { updateMediaRow(id); }

void ConfigureModeScreen::onLogMessage(const QString& formatted) {
    m_logPanel->append(formatted);
    m_logPanel->verticalScrollBar()->setValue(m_logPanel->verticalScrollBar()->maximum());
}

// ---------- stage ------------------------------------------------------------

void ConfigureModeScreen::setStageWindow(StageWindow* stage) {
    m_stageWindow = stage;
    if (!stage) return;
    connect(stage, &StageWindow::activated,  this, &ConfigureModeScreen::updateStageStatus);
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
        m_screenCombo->addItem(
            QString("Pantalla %1: %2 (%3×%4)")
                .arg(i + 1).arg(screens[i]->name())
                .arg(geo.width()).arg(geo.height()));
    }
    if (saved >= 0 && saved < m_screenCombo->count())
        m_screenCombo->setCurrentIndex(saved);

    const bool multi = screens.size() > 1;
    m_screenCombo->setVisible(multi);
    m_stageActivateBtn->setVisible(multi);
    if (!multi) m_stageStatusLabel->setText("Sin segunda pantalla");
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
    if (QGuiApplication::screens().size() <= 1) {
        m_stageStatusLabel->setText("Sin segunda pantalla");
        return;
    }
    if (!m_stageWindow || !m_stageWindow->isActive()) {
        m_stageActivateBtn->setText("Activar");
        m_stageStatusLabel->setText("Inactivo");
    } else {
        m_stageActivateBtn->setText("Desactivar");
        m_stageStatusLabel->setText(
            QString("Activo · Pantalla %1").arg(m_stageWindow->activeScreenIndex() + 1));
    }
}

void ConfigureModeScreen::loadStageConfig() {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
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

void ConfigureModeScreen::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
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
    updateStageStatus();
}
