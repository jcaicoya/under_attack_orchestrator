#include "RehearsalModeScreen.h"
#include "DefaultConfigUtils.h"
#include "Logger.h"
#include "CyberTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QShowEvent>
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
        padding: 8px 10px;
        font-weight: 700;
        font-size: 12px;
        letter-spacing: 0.5px;
    }
)QSS");

static QWidget* makeLaunchCell(QPushButton* demoBtn, QPushButton* liveBtn, QWidget* parent) {
    auto* w = new QWidget(parent);
    w->setStyleSheet("background: transparent;");
    auto* lay = new QHBoxLayout(w);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(6);
    demoBtn->setMinimumSize(88, 36);
    liveBtn->setMinimumSize(88, 36);
    lay->addWidget(demoBtn);
    lay->addWidget(liveBtn);
    lay->addStretch();
    return w;
}

static void setCellButtonsEnabled(QTableWidget* table, int row, int col, bool enabled) {
    QWidget* cell = table->cellWidget(row, col);
    if (!cell) return;
    if (auto* button = qobject_cast<QPushButton*>(cell)) { button->setEnabled(enabled); return; }
    for (auto* button : cell->findChildren<QPushButton*>())
        button->setEnabled(enabled);
}

// ---------- construction -----------------------------------------------------

RehearsalModeScreen::RehearsalModeScreen(const QString& packageRoot, QWidget* parent)
    : QWidget(parent)
    , m_packageRoot(packageRoot)
    , m_appManager(new AppManager(packageRoot, this))
    , m_adb(new AdbManager(this))
    , m_androidManager(new AndroidManager(m_adb, this))
    , m_mediaManager(new MediaManager(packageRoot, this))
{
    setFocusPolicy(Qt::StrongFocus);

    connect(m_appManager,     &AppManager::stateChanged,     this, &RehearsalModeScreen::onAppStateChanged);
    connect(m_appManager,     &AppManager::logMessage,       &Logger::instance(), &Logger::log);
    connect(m_androidManager, &AndroidManager::stateChanged, this, &RehearsalModeScreen::onAndroidStateChanged);
    connect(m_androidManager, &AndroidManager::logMessage,   &Logger::instance(), &Logger::log);
    connect(m_adb,            &AdbManager::log,              &Logger::instance(), &Logger::log);
    connect(m_adb, &AdbManager::deviceFound, this, [this](const QString& serial) {
        if (m_adbStatusLabel) m_adbStatusLabel->setText(QString("● %1").arg(serial));
    });
    connect(m_adb, &AdbManager::deviceLost, this, [this]() {
        if (m_adbStatusLabel) m_adbStatusLabel->setText("○ Sin dispositivo");
    });
    connect(m_mediaManager,   &MediaManager::stateChanged,   this, &RehearsalModeScreen::onMediaStateChanged);
    connect(m_mediaManager,   &MediaManager::logMessage,     &Logger::instance(), &Logger::log);
    connect(&Logger::instance(), &Logger::messageLogged,     this, &RehearsalModeScreen::onLogMessage);

    buildUI();

    connect(qGuiApp, &QGuiApplication::screenAdded,   this, [this](QScreen*) { populateScreenCombo(); loadStageConfig(); updateStageControls(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) { populateScreenCombo(); loadStageConfig(); updateStageControls(); });
}

// ---------- UI ---------------------------------------------------------------

void RehearsalModeScreen::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(16, 14, 16, 14);

    // Header
    auto* header = new QHBoxLayout();
    auto* title = new QLabel("ENSAYO", this);
    title->setObjectName("ScreenTitle");
    header->addWidget(title);
    header->addStretch();
    auto* hint = new QLabel("1 · Configurar   3 · Show   ←  →  Cambiar modo   Esc · Selector", this);
    hint->setObjectName("MutedLabel");
    header->addWidget(hint);
    root->addLayout(header);

    // Stage controls
    auto* stageBar = new QHBoxLayout();
    stageBar->setSpacing(8);
    auto* stageLabel = new QLabel("Escenario", this);
    stageLabel->setObjectName("FieldLabel");
    stageBar->addWidget(stageLabel);
    m_screenCombo = new QComboBox(this);
    m_screenCombo->setFocusPolicy(Qt::NoFocus);
    m_screenCombo->setMinimumWidth(130);
    stageBar->addWidget(m_screenCombo);
    m_stageActivateBtn = new QPushButton("Activar", this);
    m_stageActivateBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_stageActivateBtn, &QPushButton::clicked, this, &RehearsalModeScreen::onActivateStage);
    stageBar->addWidget(m_stageActivateBtn);
    stageBar->addStretch();
    root->addLayout(stageBar);

    // Rundown table (Qt apps + Android + media)
    auto* rundownLabel = new QLabel("Rundown", this);
    rundownLabel->setObjectName("FieldLabel");
    root->addWidget(rundownLabel);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({"", "Tipo", "Nombre", "Iniciar", "Parar", "Estado"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed); m_table->setColumnWidth(0, 72);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); m_table->setColumnWidth(1, 92);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); m_table->setColumnWidth(2, 300);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); m_table->setColumnWidth(3, 220);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed); m_table->setColumnWidth(4, 110);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setSortingEnabled(false);
    m_table->setStyleSheet(TABLE_STYLE);
    m_table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    root->addWidget(m_table, 2);

    auto* androidRow = new QHBoxLayout();
    androidRow->setSpacing(12);
    m_adbStatusLabel = new QLabel("○ Sin dispositivo", this);
    m_adbStatusLabel->setObjectName("MutedLabel");
    androidRow->addWidget(m_adbStatusLabel);
    androidRow->addStretch();
    root->addLayout(androidRow);

    // Log
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
        "}");
    m_logPanel->setFixedHeight(140);
    m_logPanel->setVisible(false);
    root->addWidget(m_logPanel);
}

// ---------- stage ------------------------------------------------------------

void RehearsalModeScreen::setStageWindow(StageWindow* stage) {
    m_stageWindow = stage;
    if (!stage) return;

    connect(stage, &StageWindow::activated, this, [this](int idx) {
        const auto screens = QGuiApplication::screens();
        if (idx < screens.size())
            m_mediaManager->setStageGeometry(screens[idx]->geometry());
        m_appManager->setStageScreen(idx);
        m_stageActivateBtn->setText("Desactivar");
        m_stageWindow->showLogo();
        saveStageConfig(idx);
    });
    connect(stage, &StageWindow::deactivated, this, [this]() {
        m_mediaManager->setStageGeometry({});
        m_appManager->setStageScreen(-1);
        m_stageActivateBtn->setText("Activar");
    });
}

void RehearsalModeScreen::populateScreenCombo() {
    const int saved = m_screenCombo->currentData().toInt();
    m_screenCombo->blockSignals(true);
    m_screenCombo->clear();
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i)
        m_screenCombo->addItem(
            QString("Pantalla %1 (%2×%3)").arg(i + 1)
                .arg(screens[i]->geometry().width())
                .arg(screens[i]->geometry().height()), i);
    if (m_screenCombo->count() > 0) {
        int def = m_screenCombo->count() - 1;
        for (int i = 0; i < m_screenCombo->count(); ++i)
            if (m_screenCombo->itemData(i).toInt() == saved) { def = i; break; }
        m_screenCombo->setCurrentIndex(def);
    }
    m_screenCombo->blockSignals(false);

    const bool multi = screens.size() > 1;
    m_screenCombo->setVisible(multi);
    m_stageActivateBtn->setVisible(multi);
}

void RehearsalModeScreen::onActivateStage() {
    if (!m_stageWindow) return;
    if (m_stageWindow->isActive()) {
        m_stageWindow->deactivate();
    } else {
        m_stageWindow->activateOnScreen(m_screenCombo->currentData().toInt());
    }
}

void RehearsalModeScreen::loadStageConfig() {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
    if (!QFileInfo::exists(path))
        DefaultConfigUtils::copyResourceDefaultTo(":/defaults/resources/stage.json", path);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const int idx = QJsonDocument::fromJson(f.readAll()).object().value("screenIndex").toInt(0);
    for (int i = 0; i < m_screenCombo->count(); ++i)
        if (m_screenCombo->itemData(i).toInt() == idx) { m_screenCombo->setCurrentIndex(i); break; }
}

void RehearsalModeScreen::saveStageConfig(int screenIndex) {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
    QJsonObject obj;
    obj["screenIndex"] = screenIndex;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(obj).toJson());
}

void RehearsalModeScreen::updateStageControls() {
    if (QGuiApplication::screens().size() <= 1) return;
    const bool active = m_stageWindow && m_stageWindow->isActive();
    m_stageActivateBtn->setText(active ? "Desactivar" : "Activar");
    m_screenCombo->setEnabled(!active);
    if (!active) {
        m_mediaManager->setStageGeometry({});
        m_appManager->setStageScreen(-1);
    } else {
        const int idx = m_stageWindow->activeScreenIndex();
        const auto screens = QGuiApplication::screens();
        if (idx < screens.size())
            m_mediaManager->setStageGeometry(screens[idx]->geometry());
        m_appManager->setStageScreen(idx);
    }
}

// ---------- data sync --------------------------------------------------------

void RehearsalModeScreen::syncAndRefresh() {
    // Qt apps
    const QString appPath = QDir(m_packageRoot).filePath("config/apps.json");
    if (QFileInfo::exists(appPath)) m_appConfig.loadFromFile(appPath);
    m_appManager->loadApps(m_appConfig.apps());

    // Android apps
    const QString androidPath = QDir(m_packageRoot).filePath("config/android.json");
    if (QFileInfo::exists(androidPath)) m_androidConfig.loadFromFile(androidPath);
    m_androidManager->loadApps(m_androidConfig.apps());

    // Media
    const QString mediaPath = QDir(m_packageRoot).filePath("config/media.json");
    m_mediaConfig.loadFromFile(mediaPath);
    m_mediaManager->loadMedia(m_mediaConfig.items());

    // Re-wire stage geometry after reload
    if (m_stageWindow && m_stageWindow->isActive()) {
        const int idx = m_stageWindow->activeScreenIndex();
        const auto screens = QGuiApplication::screens();
        if (idx >= 0 && idx < screens.size())
            m_mediaManager->setStageGeometry(screens[idx]->geometry());
    }

    // Rundown
    m_rundownPath = QDir(m_packageRoot).filePath("config/rundown.json");
    if (!QFileInfo::exists(m_rundownPath))
        RundownConfig::copyDefaultTo(m_rundownPath);
    m_rundownConfig.loadFromFile(m_rundownPath);
    QStringList appIds, androidIds, mediaIds;
    for (const auto& e : m_appConfig.apps())    appIds   << e.id;
    for (const auto& e : m_androidConfig.apps()) androidIds << e.id;
    for (const auto& e : m_mediaConfig.items()) mediaIds << e.id;
    m_rundownConfig.syncWithLibraries(appIds, androidIds, mediaIds);
    m_rundownConfig.saveToFile(m_rundownPath);

    populateTable();
    Logger::instance().log(QString("Ensayo: %1 elemento(s) en el rundown, %2 app(s) Android.")
        .arg(m_rundownConfig.items().size()).arg(m_androidConfig.apps().size()));
}

// ---------- rundown table ----------------------------------------------------

void RehearsalModeScreen::populateTable() {
    m_table->setRowCount(0);
    const auto& items = m_rundownConfig.items();

    for (int row = 0; row < items.size(); ++row) {
        const RundownItem& item = items[row];
        m_table->insertRow(row);

        // Col 0: ▲▼
        auto* arrowWidget = new QWidget(this);
        arrowWidget->setStyleSheet("background: transparent;");
        auto* arrowLay = new QHBoxLayout(arrowWidget);
        arrowLay->setContentsMargins(2, 2, 2, 2);
        arrowLay->setSpacing(4);
        auto* upBtn   = new QPushButton("▲", this);
        auto* downBtn = new QPushButton("▼", this);
        upBtn->setFixedSize(28, 34);   downBtn->setFixedSize(28, 34);
        upBtn->setFocusPolicy(Qt::NoFocus);   downBtn->setFocusPolicy(Qt::NoFocus);
        upBtn->setEnabled(row > 0);
        downBtn->setEnabled(row < items.size() - 1);
        upBtn->setStyleSheet("QPushButton { font-size: 11px; padding: 0; }");
        downBtn->setStyleSheet("QPushButton { font-size: 11px; padding: 0; }");
        arrowLay->addWidget(upBtn);
        arrowLay->addWidget(downBtn);
        arrowLay->addStretch();
        m_table->setCellWidget(row, 0, arrowWidget);

        connect(upBtn, &QPushButton::clicked, this, [this, row]() {
            m_rundownConfig.moveUp(row); m_rundownConfig.saveToFile(m_rundownPath); populateTable();
        });
        connect(downBtn, &QPushButton::clicked, this, [this, row]() {
            m_rundownConfig.moveDown(row); m_rundownConfig.saveToFile(m_rundownPath); populateTable();
        });

        // Col 1: Tipo
        QString typeStr;
        QColor  typeColor;
        if (item.type == "app") {
            typeStr = "APP"; typeColor = QColor("#A0C8FF");
        } else if (item.type == "android") {
            typeStr = "ANDROID"; typeColor = QColor("#80E0A0");
        } else {
            bool isVideo = false;
            if (const auto* e = mediaEntryForId(item.ref)) isVideo = (e->type == "video");
            typeStr   = isVideo ? "VIDEO" : "AUDIO";
            typeColor = isVideo ? QColor("#00BFFF") : QColor("#FF8C00");
        }
        auto* typeItem = new QTableWidgetItem(typeStr);
        typeItem->setForeground(typeColor);
        typeItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_table->setItem(row, 1, typeItem);

        // Col 2: Nombre
        QString name = item.ref;
        if (item.type == "app") {
            if (const auto* e = appEntryForId(item.ref)) name = e->name;
        } else if (item.type == "android") {
            if (const auto* e = androidEntryForId(item.ref)) name = e->name;
        } else {
            if (const auto* e = mediaEntryForId(item.ref)) name = e->name;
        }
        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setForeground(CyberTheme::color(CyberTheme::TextPrimary));
        nameItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_table->setItem(row, 2, nameItem);

        // Col 3: Iniciar
        const QString ref  = item.ref;
        const QString type = item.type;
        if (type == "app") {
            auto* demoBtn = new QPushButton("Demo", this);
            auto* liveBtn = new QPushButton("Live", this);
            demoBtn->setFocusPolicy(Qt::NoFocus);
            liveBtn->setFocusPolicy(Qt::NoFocus);
            m_table->setCellWidget(row, 3, makeLaunchCell(demoBtn, liveBtn, this));
            connect(demoBtn, &QPushButton::clicked, this, [this, ref]() { m_appManager->start(ref, AppLaunchMode::Demo); });
            connect(liveBtn, &QPushButton::clicked, this, [this, ref]() { m_appManager->start(ref, AppLaunchMode::Live); });
        } else if (type == "android") {
            auto* actionBtn = new QPushButton("Lanzar", this);
            actionBtn->setFocusPolicy(Qt::NoFocus);
            m_table->setCellWidget(row, 3, actionBtn);
            connect(actionBtn, &QPushButton::clicked, this, [this, ref]() { m_androidManager->start(ref); });
        } else {
            auto* actionBtn = new QPushButton("Iniciar", this);
            actionBtn->setFocusPolicy(Qt::NoFocus);
            m_table->setCellWidget(row, 3, actionBtn);
            connect(actionBtn, &QPushButton::clicked, this, [this, ref]() { m_mediaManager->play(ref); });
        }

        // Col 4: Parar
        auto* stopBtn = new QPushButton("Parar", this);
        stopBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setMinimumSize(88, 36);
        stopBtn->setEnabled(false);
        m_table->setCellWidget(row, 4, stopBtn);
        connect(stopBtn, &QPushButton::clicked, this, [this, ref, type]() {
            if (type == "app")          m_appManager->stop(ref);
            else if (type == "android") m_androidManager->stop(ref);
            else                        m_mediaManager->stop(ref);
        });

        // Col 5: Estado
        auto* stateItem = new QTableWidgetItem("LISTA");
        stateItem->setForeground(CyberTheme::color(CyberTheme::TextMuted));
        m_table->setItem(row, 5, stateItem);

        m_table->setRowHeight(row, 52);
        updateRow(row);
    }
}

void RehearsalModeScreen::updateRow(int row) {
    const auto& items = m_rundownConfig.items();
    if (row < 0 || row >= items.size()) return;
    const RundownItem& item = items[row];

    QString stateLabel;
    QColor  stateColor;
    bool    canAction = false;
    bool    canStop   = false;

    if (item.type == "app") {
        AppState s = m_appManager->state(item.ref);
        stateLabel = appStateLabel(s);
        stateColor = appStateColor(s);
        canAction  = (s == AppState::Stopped || s == AppState::Error);
        canStop    = (s == AppState::Starting || s == AppState::Running || s == AppState::Stopping);
    } else if (item.type == "android") {
        AndroidState s = m_androidManager->state(item.ref);
        stateLabel = androidStateLabel(s);
        stateColor = androidStateColor(s);
        canAction  = (s == AndroidState::Stopped);
        canStop    = (s == AndroidState::Running);
    } else {
        MediaState s = m_mediaManager->state(item.ref);
        stateLabel = mediaStateLabel(s);
        stateColor = mediaStateColor(s);
        canAction  = (s == MediaState::Stopped || s == MediaState::Error);
        canStop    = (s == MediaState::Playing);
    }

    if (auto* si = m_table->item(row, 5)) {
        si->setText(stateLabel);
        si->setForeground(stateColor);
        si->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    }
    setCellButtonsEnabled(m_table, row, 3, canAction);
    if (auto* b = qobject_cast<QPushButton*>(m_table->cellWidget(row, 4))) b->setEnabled(canStop);
}

int RehearsalModeScreen::rowForRef(const QString& type, const QString& id) const {
    const auto& items = m_rundownConfig.items();
    for (int i = 0; i < items.size(); ++i)
        if (items[i].type == type && items[i].ref == id) return i;
    return -1;
}

const AppEntry* RehearsalModeScreen::appEntryForId(const QString& id) const {
    for (const auto& e : m_appConfig.apps())
        if (e.id == id) return &e;
    return nullptr;
}

const AndroidEntry* RehearsalModeScreen::androidEntryForId(const QString& id) const {
    for (const auto& e : m_androidConfig.apps())
        if (e.id == id) return &e;
    return nullptr;
}

const MediaEntry* RehearsalModeScreen::mediaEntryForId(const QString& id) const {
    for (const auto& e : m_mediaConfig.items())
        if (e.id == id) return &e;
    return nullptr;
}

// ---------- slots ------------------------------------------------------------

void RehearsalModeScreen::onAppStateChanged(const QString& id, AppState state) {
    int row = rowForRef("app", id);
    if (row >= 0) updateRow(row);
    if (m_stageWindow && m_stageWindow->isActive()) {
        if (state == AppState::Running)
            m_stageWindow->softHide();
        else if (state == AppState::Stopped || state == AppState::Error) {
            m_stageWindow->softShow();
            m_stageWindow->showLogo();
        }
    }
}

void RehearsalModeScreen::onAndroidStateChanged(const QString& id, AndroidState) {
    int row = rowForRef("android", id);
    if (row >= 0) updateRow(row);
}

void RehearsalModeScreen::onMediaStateChanged(const QString& id, MediaState state) {
    int row = rowForRef("media", id);
    if (row >= 0) {
        updateRow(row);
        if (m_stageWindow && m_stageWindow->isActive()) {
            if (const auto* e = mediaEntryForId(id); e && e->type == "video") {
                if (state == MediaState::Stopped || state == MediaState::Error)
                    m_stageWindow->showLogo();
            }
        }
    }
}

void RehearsalModeScreen::onLogMessage(const QString& formatted) {
    m_logPanel->append(formatted);
    m_logPanel->verticalScrollBar()->setValue(m_logPanel->verticalScrollBar()->maximum());
}

void RehearsalModeScreen::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_F10:    m_logPanel->setVisible(!m_logPanel->isVisible()); break;
        case Qt::Key_Escape:               emit returnToSelector(); break;
        case Qt::Key_1: case Qt::Key_Left: emit switchMode(0); break;
        case Qt::Key_3: case Qt::Key_Right: emit switchMode(2); break;
        default: QWidget::keyPressEvent(event);
    }
}

void RehearsalModeScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setFocus();
    populateScreenCombo();
    loadStageConfig();
    updateStageControls();
    syncAndRefresh();
    m_adb->detectDevice();
}
