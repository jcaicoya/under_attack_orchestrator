#include "ShowModeScreen.h"
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
        case AndroidState::Stopped:    return "PARADA";
        case AndroidState::Foreground: return "EN PRIMER PLANO";
        case AndroidState::Background: return "EN SEGUNDO PLANO";
    }
    return {};
}

static QColor androidStateColor(AndroidState s) {
    switch (s) {
        case AndroidState::Stopped:    return CyberTheme::color(CyberTheme::TextMuted);
        case AndroidState::Foreground: return CyberTheme::color(CyberTheme::AccentGreen);
        case AndroidState::Background: return CyberTheme::color(CyberTheme::Warning);
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
        padding: 6px 10px;
        border-bottom: 1px solid #1A2030;
    }
    QTableWidget::item:selected {
        background-color: #1A2A3A;
        color: #E0E8F0;
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

ShowModeScreen::ShowModeScreen(const QString& packageRoot, QWidget* parent)
    : QWidget(parent)
    , m_packageRoot(packageRoot)
    , m_appManager(new AppManager(packageRoot, this))
    , m_adb(new AdbManager(this))
    , m_androidManager(new AndroidManager(m_adb, this))
    , m_mediaManager(new MediaManager(packageRoot, this))
{
    setFocusPolicy(Qt::StrongFocus);

    connect(m_appManager,   &AppManager::stateChanged,   this, &ShowModeScreen::onStateChanged);
    connect(m_appManager,   &AppManager::logMessage,     &Logger::instance(), &Logger::log);
    connect(m_androidManager, &AndroidManager::stateChanged, this, &ShowModeScreen::onAndroidStateChanged);
    connect(m_androidManager, &AndroidManager::logMessage, &Logger::instance(), &Logger::log);
    connect(m_adb, &AdbManager::log, &Logger::instance(), &Logger::log);
    connect(m_mediaManager, &MediaManager::stateChanged, this, &ShowModeScreen::onMediaStateChanged);
    connect(m_mediaManager, &MediaManager::logMessage,   &Logger::instance(), &Logger::log);
    connect(&Logger::instance(), &Logger::messageLogged, this, &ShowModeScreen::onLogMessage);

    buildUI();

    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen*) {
        populateScreenCombo();
        loadStageConfig();
        updateStageControls();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this](QScreen*) {
        populateScreenCombo();
        loadStageConfig();
        updateStageControls();
    });
}

// ---------- UI ---------------------------------------------------------------

void ShowModeScreen::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(16, 14, 16, 14);

    // Header
    auto* header = new QHBoxLayout();
    auto* title = new QLabel("SHOW", this);
    title->setObjectName("ScreenTitle");
    header->addWidget(title);
    header->addStretch();
    auto* escHint = new QLabel("← / 2 · Ensayo   1 · Configurar   → / Espacio · Siguiente   Esc · Selector", this);
    escHint->setObjectName("MutedLabel");
    header->addWidget(escHint);
    root->addLayout(header);

    // ── Stage controls ────────────────────────────────────────────────────────
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
    connect(m_stageActivateBtn, &QPushButton::clicked, this, &ShowModeScreen::onActivateStage);
    stageBar->addWidget(m_stageActivateBtn);

    stageBar->addStretch();
    root->addLayout(stageBar);

    // ── Scene table ───────────────────────────────────────────────────────────
    // Columns: Nº | Tipo | Escena | Estado
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({"Nº", "Tipo", "Escena", "Estado"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed); m_table->setColumnWidth(0, 42);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed); m_table->setColumnWidth(1, 78);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); m_table->setColumnWidth(3, 130);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setSortingEnabled(false);
    m_table->setStyleSheet(TABLE_STYLE);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) { activateScene(row); });
    root->addWidget(m_table, 1);

    // ── Navigation bar ────────────────────────────────────────────────────────
    auto* navBar = new QHBoxLayout();
    navBar->setSpacing(8);

    m_prevBtn = new QPushButton("◀  Anterior", this);
    m_prevBtn->setFocusPolicy(Qt::NoFocus);
    m_prevBtn->setMinimumWidth(110);
    m_prevBtn->setEnabled(false);
    connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
        if (m_currentRow > 0) activateScene(m_currentRow - 1);
    });
    navBar->addWidget(m_prevBtn);

    m_activateBtn = new QPushButton("▶  Activar", this);
    m_activateBtn->setFocusPolicy(Qt::NoFocus);
    m_activateBtn->setMinimumWidth(110);
    connect(m_activateBtn, &QPushButton::clicked, this, [this]() {
        const int target = (m_currentRow < 0) ? 0 : m_currentRow;
        if (target < m_table->rowCount()) activateScene(target);
    });
    navBar->addWidget(m_activateBtn);

    m_nextBtn = new QPushButton("Siguiente  ▶", this);
    m_nextBtn->setFocusPolicy(Qt::NoFocus);
    m_nextBtn->setMinimumWidth(110);
    m_nextBtn->setEnabled(false);
    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        const int next = m_currentRow + 1;
        if (next < m_table->rowCount()) activateScene(next);
    });
    navBar->addWidget(m_nextBtn);

    navBar->addStretch();

    m_sceneLabel = new QLabel("—", this);
    m_sceneLabel->setObjectName("MutedLabel");
    navBar->addWidget(m_sceneLabel);

    root->addLayout(navBar);

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
    m_logPanel->setFixedHeight(120);
    m_logPanel->setVisible(false);
    root->addWidget(m_logPanel);
}

// ---------- stage ------------------------------------------------------------

void ShowModeScreen::setStageWindow(StageWindow* stage) {
    m_stageWindow = stage;
    if (!stage) return;

    connect(stage, &StageWindow::activated, this, [this](int idx) {
        const auto screens = QGuiApplication::screens();
        if (idx < screens.size()) {
            m_mediaManager->setStageGeometry(screens[idx]->geometry());
        }
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

void ShowModeScreen::populateScreenCombo() {
    const int saved = m_screenCombo->currentData().toInt();
    m_screenCombo->blockSignals(true);
    m_screenCombo->clear();
    const auto screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i)
        m_screenCombo->addItem(
            QString("Pantalla %1 (%2×%3)").arg(i + 1)
                .arg(screens[i]->geometry().width())
                .arg(screens[i]->geometry().height()),
            i);
    if (m_screenCombo->count() > 0) {
        int defaultIdx = m_screenCombo->count() - 1;
        for (int i = 0; i < m_screenCombo->count(); ++i)
            if (m_screenCombo->itemData(i).toInt() == saved) { defaultIdx = i; break; }
        m_screenCombo->setCurrentIndex(defaultIdx);
    }
    m_screenCombo->blockSignals(false);

    const bool multi = screens.size() > 1;
    m_screenCombo->setVisible(multi);
    m_stageActivateBtn->setVisible(multi);
}

void ShowModeScreen::onActivateStage() {
    if (!m_stageWindow) return;
    if (m_stageWindow->isActive()) {
        m_stageWindow->deactivate();
    } else {
        int screenIdx = m_screenCombo->currentData().toInt();
        m_stageWindow->activateOnScreen(screenIdx);
    }
}

void ShowModeScreen::loadStageConfig() {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
    if (!QFileInfo::exists(path))
        DefaultConfigUtils::copyResourceDefaultTo(":/defaults/resources/stage.json", path);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto obj = QJsonDocument::fromJson(f.readAll()).object();
    int idx = obj.value("screenIndex").toInt(0);
    for (int i = 0; i < m_screenCombo->count(); ++i) {
        if (m_screenCombo->itemData(i).toInt() == idx) {
            m_screenCombo->setCurrentIndex(i);
            break;
        }
    }
}

void ShowModeScreen::saveStageConfig(int screenIndex) {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
    QJsonObject obj;
    obj["screenIndex"] = screenIndex;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}

void ShowModeScreen::updateStageControls() {
    if (QGuiApplication::screens().size() <= 1)
        return;
    const bool active = m_stageWindow && m_stageWindow->isActive();
    m_stageActivateBtn->setText(active ? "Desactivar" : "Activar");
    m_screenCombo->setEnabled(!active);
    if (!active) {
        m_mediaManager->setStageGeometry({});
        m_appManager->setStageScreen(-1);
    } else {
        const int idx = m_stageWindow->activeScreenIndex();
        const auto screens = QGuiApplication::screens();
        if (idx < screens.size()) {
            m_mediaManager->setStageGeometry(screens[idx]->geometry());
        }
        m_appManager->setStageScreen(idx);
    }
}

// ---------- data load --------------------------------------------------------

void ShowModeScreen::loadAndSync() {
    const QString appConfigPath = QDir(m_packageRoot).filePath("config/apps.json");
    if (QFileInfo::exists(appConfigPath))
        m_appConfig.loadFromFile(appConfigPath);
    m_appManager->loadApps(m_appConfig.apps());

    const QString androidConfigPath = QDir(m_packageRoot).filePath("config/android.json");
    if (QFileInfo::exists(androidConfigPath))
        m_androidConfig.loadFromFile(androidConfigPath);
    m_androidManager->loadApps(m_androidConfig.apps());

    const QString mediaConfigPath = QDir(m_packageRoot).filePath("config/media.json");
    m_mediaConfig.loadFromFile(mediaConfigPath);
    m_mediaManager->loadMedia(m_mediaConfig.items());

    if (m_stageWindow && m_stageWindow->isActive()) {
        const int idx = m_stageWindow->activeScreenIndex();
        const auto screens = QGuiApplication::screens();
        if (idx >= 0 && idx < screens.size())
            m_mediaManager->setStageGeometry(screens[idx]->geometry());
    }

    m_rundownPath = QDir(m_packageRoot).filePath("config/rundown.json");
    if (!QFileInfo::exists(m_rundownPath))
        RundownConfig::copyDefaultTo(m_rundownPath);
    RundownConfig full;
    full.loadFromFile(m_rundownPath);

    // Show mode uses all items
    m_rundownConfig.setItems({});
    QList<RundownItem> enabled;
    for (const auto& item : full.items())
        enabled.append(item);
    m_rundownConfig.setItems(enabled);

    m_currentRow = -1;
    populateTable();
    updateNavButtons();

    Logger::instance().log(
        QString("Show: %1 escena(s) en el rundown.").arg(m_rundownConfig.items().size()));
}

// ---------- table ------------------------------------------------------------

void ShowModeScreen::populateTable() {
    m_table->setRowCount(0);
    const auto& items = m_rundownConfig.items();

    for (int row = 0; row < items.size(); ++row) {
        const RundownItem& item = items[row];
        m_table->insertRow(row);

        // Col 0: Nº
        auto* numItem = new QTableWidgetItem(QString::number(row + 1));
        numItem->setForeground(CyberTheme::color(CyberTheme::TextMuted));
        numItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 0, numItem);

        // Col 1: Tipo
        QString typeStr;
        QColor  typeColor;
        if (item.type == "app") {
            typeStr   = "APP";
            typeColor = QColor("#A0C8FF");
        } else if (item.type == "android") {
            typeStr   = "ANDROID";
            typeColor = QColor("#80E0A0");
        } else {
            bool isVideo = false;
            if (const auto* e = mediaEntryForId(item.ref)) isVideo = (e->type == "video");
            typeStr   = isVideo ? "VIDEO" : "AUDIO";
            typeColor = isVideo ? QColor("#00BFFF") : QColor("#FF8C00");
        }
        auto* typeItem = new QTableWidgetItem(typeStr);
        typeItem->setForeground(typeColor);
        typeItem->setTextAlignment(Qt::AlignCenter);
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
        m_table->setItem(row, 2, nameItem);

        // Col 3: Estado
        auto* stateItem = new QTableWidgetItem("LISTA");
        stateItem->setForeground(CyberTheme::color(CyberTheme::TextMuted));
        m_table->setItem(row, 3, stateItem);

        m_table->setRowHeight(row, 42);
        updateRow(row);
    }
}

void ShowModeScreen::updateRow(int row) {
    const auto& items = m_rundownConfig.items();
    if (row < 0 || row >= items.size()) return;
    const RundownItem& item = items[row];

    QString stateLabel;
    QColor  stateColor;

    if (item.type == "app") {
        AppState s = m_appManager->state(item.ref);
        stateLabel = appStateLabel(s);
        stateColor = appStateColor(s);
    } else if (item.type == "android") {
        AndroidState s = m_androidManager->state(item.ref);
        stateLabel = androidStateLabel(s);
        stateColor = androidStateColor(s);
    } else {
        MediaState s = m_mediaManager->state(item.ref);
        stateLabel = mediaStateLabel(s);
        stateColor = mediaStateColor(s);
    }

    if (auto* si = m_table->item(row, 3)) {
        si->setText(stateLabel);
        si->setForeground(stateColor);
    }

    // Highlight active row
    const bool isActive = (row == m_currentRow);
    const QColor bg     = isActive ? QColor("#0D2235") : QColor();
    const QColor accent = isActive ? QColor("#00BFFF") : CyberTheme::color(CyberTheme::TextPrimary);
    for (int col = 0; col < m_table->columnCount(); ++col) {
        if (auto* it = m_table->item(row, col)) {
            it->setBackground(isActive ? bg : QBrush());
            if (col == 2) it->setForeground(accent);
        }
    }
    if (auto* numIt = m_table->item(row, 0))
        numIt->setForeground(isActive ? QColor("#00BFFF") : CyberTheme::color(CyberTheme::TextMuted));
}

void ShowModeScreen::updateNavButtons() {
    const int count = m_table->rowCount();
    m_prevBtn->setEnabled(m_currentRow > 0);
    m_nextBtn->setEnabled(m_currentRow >= 0 && m_currentRow < count - 1);
    m_activateBtn->setEnabled(count > 0);

    if (m_currentRow < 0) {
        m_sceneLabel->setText(count > 0 ? "Selecciona una escena" : "Sin escenas");
    } else {
        const QString name = m_table->item(m_currentRow, 2)
            ? m_table->item(m_currentRow, 2)->text() : QString();
        const int next = m_currentRow + 1;
        const QString nextName = (next < count && m_table->item(next, 2))
            ? m_table->item(next, 2)->text() : QString("—");
        m_sceneLabel->setText(
            QString("Escena %1/%2: %3   →  %4").arg(m_currentRow + 1).arg(count).arg(name, nextName));
    }
}

int ShowModeScreen::rowForRef(const QString& type, const QString& id) const {
    const auto& items = m_rundownConfig.items();
    for (int i = 0; i < items.size(); ++i)
        if (items[i].type == type && items[i].ref == id) return i;
    return -1;
}

const AppEntry* ShowModeScreen::appEntryForId(const QString& id) const {
    for (const auto& e : m_appConfig.apps())
        if (e.id == id) return &e;
    return nullptr;
}

const AndroidEntry* ShowModeScreen::androidEntryForId(const QString& id) const {
    for (const auto& e : m_androidConfig.apps())
        if (e.id == id) return &e;
    return nullptr;
}

const MediaEntry* ShowModeScreen::mediaEntryForId(const QString& id) const {
    for (const auto& e : m_mediaConfig.items())
        if (e.id == id) return &e;
    return nullptr;
}

// ---------- scene control ----------------------------------------------------

void ShowModeScreen::activateScene(int row) {
    const auto& items = m_rundownConfig.items();
    if (row < 0 || row >= items.size()) return;

    // Stop previous scene if different
    if (m_currentRow >= 0 && m_currentRow != row)
        stopCurrentScene();

    const int prev = m_currentRow;
    m_currentRow = row;

    // Deselect previous row highlight
    if (prev >= 0 && prev < m_table->rowCount())
        updateRow(prev);

    const RundownItem& item = items[row];

    QString name = item.ref;
    if (item.type == "app") {
        if (const auto* e = appEntryForId(item.ref)) name = e->name;
        m_appManager->start(item.ref);
    } else if (item.type == "android") {
        if (const auto* e = androidEntryForId(item.ref)) name = e->name;
        m_androidManager->start(item.ref);
    } else {
        if (const auto* e = mediaEntryForId(item.ref)) name = e->name;
        m_mediaManager->play(item.ref);
    }

    m_table->selectRow(row);
    updateRow(row);
    updateNavButtons();
    Logger::instance().log(QString("Show: activando escena %1 — %2").arg(row + 1).arg(name));
}

void ShowModeScreen::stopCurrentScene() {
    if (m_currentRow < 0) return;
    const auto& items = m_rundownConfig.items();
    if (m_currentRow >= items.size()) return;
    const RundownItem& item = items[m_currentRow];
    if (item.type == "app")          m_appManager->stop(item.ref);
    else if (item.type == "android") m_androidManager->stop(item.ref);
    else                             m_mediaManager->stop(item.ref);
}

// ---------- slots ------------------------------------------------------------

void ShowModeScreen::onStateChanged(const QString& id, AppState state) {
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

void ShowModeScreen::onMediaStateChanged(const QString& id, MediaState state) {
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

void ShowModeScreen::onAndroidStateChanged(const QString& id, AndroidState) {
    int row = rowForRef("android", id);
    if (row >= 0) updateRow(row);
}

void ShowModeScreen::onLogMessage(const QString& formatted) {
    m_logPanel->append(formatted);
    m_logPanel->verticalScrollBar()->setValue(m_logPanel->verticalScrollBar()->maximum());
}

void ShowModeScreen::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_F10:
            m_logPanel->setVisible(!m_logPanel->isVisible());
            break;
        case Qt::Key_Right:
        case Qt::Key_Space: {
            const int next = (m_currentRow < 0) ? 0 : m_currentRow + 1;
            if (next < m_table->rowCount()) activateScene(next);
            break;
        }
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            const int target = (m_currentRow < 0) ? 0 : m_currentRow;
            if (target < m_table->rowCount()) activateScene(target);
            break;
        }
        case Qt::Key_Escape: emit returnToSelector(); break;
        case Qt::Key_Left:
        case Qt::Key_2:      emit switchMode(1); break;
        case Qt::Key_1:      emit switchMode(0); break;
        default:
            QWidget::keyPressEvent(event);
    }
}

void ShowModeScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setFocus();
    populateScreenCombo();
    loadStageConfig();
    updateStageControls();
    loadAndSync();
    m_adb->detectDevice();
}
