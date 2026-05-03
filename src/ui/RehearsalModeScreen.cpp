#include "RehearsalModeScreen.h"
#include "Logger.h"
#include "CyberTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QCheckBox>
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
        padding: 6px 8px;
        font-weight: 700;
        font-size: 11px;
        letter-spacing: 0.5px;
    }
)QSS");

// ---------- construction -----------------------------------------------------

RehearsalModeScreen::RehearsalModeScreen(const QString& packageRoot, QWidget* parent)
    : QWidget(parent)
    , m_packageRoot(packageRoot)
    , m_appManager(new AppManager(packageRoot, this))
    , m_mediaManager(new MediaManager(this))
{
    setFocusPolicy(Qt::StrongFocus);
    m_appManager->setMode(ShowMode::Design);

    connect(m_appManager,   &AppManager::stateChanged,   this, &RehearsalModeScreen::onStateChanged);
    connect(m_appManager,   &AppManager::logMessage,     &Logger::instance(), &Logger::log);
    connect(m_mediaManager, &MediaManager::stateChanged, this, &RehearsalModeScreen::onMediaStateChanged);
    connect(m_mediaManager, &MediaManager::logMessage,   &Logger::instance(), &Logger::log);
    connect(&Logger::instance(), &Logger::messageLogged, this, &RehearsalModeScreen::onLogMessage);

    buildUI();
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
    auto* escHint = new QLabel("1 · Configurar   3 · Show   ←  →  Cambiar modo   Esc · Selector", this);
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
    connect(m_stageActivateBtn, &QPushButton::clicked, this, &RehearsalModeScreen::onActivateStage);
    stageBar->addWidget(m_stageActivateBtn);

    stageBar->addSpacing(12);

    m_stageBlackBtn = new QPushButton("Pantalla negra", this);
    m_stageBlackBtn->setFocusPolicy(Qt::NoFocus);
    m_stageBlackBtn->setEnabled(false);
    connect(m_stageBlackBtn, &QPushButton::clicked, this, [this]() {
        if (m_stageWindow) m_stageWindow->showBlack();
    });
    stageBar->addWidget(m_stageBlackBtn);

    m_stageLogoBtn = new QPushButton("Logo", this);
    m_stageLogoBtn->setFocusPolicy(Qt::NoFocus);
    m_stageLogoBtn->setEnabled(false);
    connect(m_stageLogoBtn, &QPushButton::clicked, this, [this]() {
        if (m_stageWindow) m_stageWindow->showLogo();
    });
    stageBar->addWidget(m_stageLogoBtn);

    m_stageStatusLabel = new QLabel("Inactivo", this);
    m_stageStatusLabel->setObjectName("MutedLabel");
    stageBar->addWidget(m_stageStatusLabel);
    stageBar->addStretch();
    root->addLayout(stageBar);

    // Rundown table — columns: "" | Nombre | Tipo | ✓ | Acción | Parar | Estado
    m_table = new QTableWidget(0, 7, this);
    m_table->setHorizontalHeaderLabels({"", "Nombre", "Tipo", "✓", "Acción", "Parar", "Estado"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed); m_table->setColumnWidth(0, 52);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed); m_table->setColumnWidth(2, 68);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed); m_table->setColumnWidth(3, 36);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed); m_table->setColumnWidth(4, 100);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed); m_table->setColumnWidth(5, 80);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed); m_table->setColumnWidth(6, 130);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setSortingEnabled(false);
    m_table->setStyleSheet(TABLE_STYLE);
    root->addWidget(m_table, 1);

    // Bottom bar
    auto* bar = new QHBoxLayout();
    bar->addStretch();
    m_stopAllBtn = new QPushButton("Parar todo", this);
    m_stopAllBtn->setObjectName("DangerButton");
    m_stopAllBtn->setMinimumWidth(110);
    m_stopAllBtn->setFocusPolicy(Qt::NoFocus);
    connect(m_stopAllBtn, &QPushButton::clicked, this, [this]() {
        m_appManager->stopAll();
        m_mediaManager->stopAll();
    });
    bar->addWidget(m_stopAllBtn);
    root->addLayout(bar);

    // Log
    auto* logLabel = new QLabel("Log:", this);
    logLabel->setObjectName("FieldLabel");
    root->addWidget(logLabel);

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
    root->addWidget(m_logPanel, 0);
    m_logPanel->setFixedHeight(120);
}

// ---------- stage ------------------------------------------------------------

void RehearsalModeScreen::setStageWindow(StageWindow* stage) {
    m_stageWindow = stage;
    if (!stage) return;

    connect(stage, &StageWindow::activated, this, [this](int idx) {
        m_mediaManager->setStageOutput(m_stageWindow->videoOutput());
        const auto screens = QGuiApplication::screens();
        if (idx < screens.size())
            m_appManager->setStageGeometry(screens[idx]->geometry());
        m_stageActivateBtn->setText("Desactivar");
        m_stageBlackBtn->setEnabled(true);
        m_stageLogoBtn->setEnabled(true);
        m_stageStatusLabel->setText(QString("Activo · Pantalla %1").arg(idx + 1));
        saveStageConfig(idx);
    });
    connect(stage, &StageWindow::deactivated, this, [this]() {
        m_mediaManager->setStageOutput(nullptr);
        m_appManager->setStageGeometry({});
        m_stageActivateBtn->setText("Activar");
        m_stageBlackBtn->setEnabled(false);
        m_stageLogoBtn->setEnabled(false);
        m_stageStatusLabel->setText("Inactivo");
    });
    connect(stage, &StageWindow::contentChanged, this, [this](StageWindow::Content c) {
        if (!m_stageWindow || !m_stageWindow->isActive()) return;
        QString suffix;
        switch (c) {
            case StageWindow::Content::Black: suffix = "Negro"; break;
            case StageWindow::Content::Logo:  suffix = "Logo";  break;
            case StageWindow::Content::Video: suffix = "Video"; break;
        }
        m_stageStatusLabel->setText(
            QString("Activo · Pantalla %1 · %2")
                .arg(m_stageWindow->activeScreenIndex() + 1).arg(suffix));
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
    m_stageBlackBtn->setVisible(multi);
    m_stageLogoBtn->setVisible(multi);
    if (!multi) m_stageStatusLabel->setText("Sin segunda pantalla");
}

void RehearsalModeScreen::onActivateStage() {
    if (!m_stageWindow) return;
    if (m_stageWindow->isActive()) {
        m_stageWindow->deactivate();
    } else {
        int screenIdx = m_screenCombo->currentData().toInt();
        m_stageWindow->activateOnScreen(screenIdx);
    }
}

void RehearsalModeScreen::loadStageConfig() {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
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

void RehearsalModeScreen::saveStageConfig(int screenIndex) {
    const QString path = QDir(m_packageRoot).filePath("config/stage.json");
    QJsonObject obj;
    obj["screenIndex"] = screenIndex;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}

void RehearsalModeScreen::updateStageControls() {
    if (QGuiApplication::screens().size() <= 1) {
        m_stageStatusLabel->setText("Sin segunda pantalla");
        return;
    }
    const bool active = m_stageWindow && m_stageWindow->isActive();
    m_stageActivateBtn->setText(active ? "Desactivar" : "Activar");
    m_stageBlackBtn->setEnabled(active);
    m_stageLogoBtn->setEnabled(active);
    m_screenCombo->setEnabled(!active);
    if (!active) {
        m_stageStatusLabel->setText("Inactivo");
        if (m_stageWindow) m_mediaManager->setStageOutput(nullptr);
    } else {
        const int idx = m_stageWindow->activeScreenIndex();
        m_stageStatusLabel->setText(QString("Activo · Pantalla %1").arg(idx + 1));
        m_mediaManager->setStageOutput(m_stageWindow->videoOutput());
        const auto screens = QGuiApplication::screens();
        if (idx < screens.size())
            m_appManager->setStageGeometry(screens[idx]->geometry());
    }
}

// ---------- data sync --------------------------------------------------------

void RehearsalModeScreen::syncAndRefresh() {
    const QString appConfigPath = QDir(m_packageRoot).filePath("config/apps.json");
    if (QFileInfo::exists(appConfigPath))
        m_appConfig.loadFromFile(appConfigPath);
    m_appManager->loadApps(m_appConfig.apps());

    const QString mediaConfigPath = QDir(m_packageRoot).filePath("config/media.json");
    m_mediaConfig.loadFromFile(mediaConfigPath);
    m_mediaManager->loadMedia(m_mediaConfig.items());

    // Ensure stage output is wired after reload
    if (m_stageWindow && m_stageWindow->isActive())
        m_mediaManager->setStageOutput(m_stageWindow->videoOutput());

    m_rundownPath = QDir(m_packageRoot).filePath("config/rundown.json");
    m_rundownConfig.loadFromFile(m_rundownPath);

    QStringList appIds, mediaIds;
    for (const auto& e : m_appConfig.apps())    appIds   << e.id;
    for (const auto& e : m_mediaConfig.items()) mediaIds << e.id;
    m_rundownConfig.syncWithLibraries(appIds, mediaIds);
    m_rundownConfig.saveToFile(m_rundownPath);

    populateTable();
    Logger::instance().log(
        QString("Ensayo: %1 elemento(s) en el rundown.").arg(m_rundownConfig.items().size()));
}

// ---------- table ------------------------------------------------------------

void RehearsalModeScreen::populateTable() {
    m_table->setRowCount(0);
    const auto& items = m_rundownConfig.items();

    for (int row = 0; row < items.size(); ++row) {
        const RundownItem& item = items[row];
        m_table->insertRow(row);

        // Col 0: ▲▼ reorder buttons
        auto* arrowWidget = new QWidget(this);
        arrowWidget->setStyleSheet("background: transparent;");
        auto* arrowLay = new QHBoxLayout(arrowWidget);
        arrowLay->setContentsMargins(2, 2, 2, 2);
        arrowLay->setSpacing(2);
        auto* upBtn   = new QPushButton("▲", this);
        auto* downBtn = new QPushButton("▼", this);
        upBtn->setFixedSize(22, 28);
        downBtn->setFixedSize(22, 28);
        upBtn->setFocusPolicy(Qt::NoFocus);
        downBtn->setFocusPolicy(Qt::NoFocus);
        upBtn->setEnabled(row > 0);
        downBtn->setEnabled(row < items.size() - 1);
        upBtn->setStyleSheet("QPushButton { font-size: 9px; padding: 0; }");
        downBtn->setStyleSheet("QPushButton { font-size: 9px; padding: 0; }");
        arrowLay->addWidget(upBtn);
        arrowLay->addWidget(downBtn);
        m_table->setCellWidget(row, 0, arrowWidget);

        connect(upBtn, &QPushButton::clicked, this, [this, row]() {
            m_rundownConfig.moveUp(row);
            m_rundownConfig.saveToFile(m_rundownPath);
            populateTable();
        });
        connect(downBtn, &QPushButton::clicked, this, [this, row]() {
            m_rundownConfig.moveDown(row);
            m_rundownConfig.saveToFile(m_rundownPath);
            populateTable();
        });

        // Col 1: Nombre
        QString name = item.ref;
        if (item.type == "app") {
            if (const auto* e = appEntryForId(item.ref)) name = e->name;
        } else {
            if (const auto* e = mediaEntryForId(item.ref)) name = e->name;
        }
        auto* nameItem = new QTableWidgetItem(name);
        nameItem->setForeground(item.enabled
            ? CyberTheme::color(CyberTheme::TextPrimary)
            : CyberTheme::color(CyberTheme::TextMuted));
        m_table->setItem(row, 1, nameItem);

        // Col 2: Tipo
        QString typeStr;
        QColor  typeColor;
        if (item.type == "app") {
            typeStr   = "APP";
            typeColor = QColor("#A0C8FF");
        } else {
            bool isVideo = false;
            if (const auto* e = mediaEntryForId(item.ref)) isVideo = (e->type == "video");
            typeStr   = isVideo ? "VIDEO" : "AUDIO";
            typeColor = isVideo ? QColor("#00BFFF") : QColor("#FF8C00");
        }
        auto* typeItem = new QTableWidgetItem(typeStr);
        typeItem->setForeground(typeColor);
        m_table->setItem(row, 2, typeItem);

        // Col 3: Enabled checkbox
        auto* cbWidget = new QWidget(this);
        cbWidget->setStyleSheet("background: transparent;");
        auto* cbLay = new QHBoxLayout(cbWidget);
        cbLay->setContentsMargins(0, 0, 0, 0);
        cbLay->setAlignment(Qt::AlignCenter);
        auto* cb = new QCheckBox(this);
        cb->setChecked(item.enabled);
        cb->setFocusPolicy(Qt::NoFocus);
        cbLay->addWidget(cb);
        m_table->setCellWidget(row, 3, cbWidget);

        connect(cb, &QCheckBox::toggled, this, [this, row](bool checked) {
            m_rundownConfig.setEnabled(row, checked);
            m_rundownConfig.saveToFile(m_rundownPath);
            if (auto* ni = m_table->item(row, 1))
                ni->setForeground(checked
                    ? CyberTheme::color(CyberTheme::TextPrimary)
                    : CyberTheme::color(CyberTheme::TextMuted));
        });

        // Col 4: Action button (Iniciar / Reproducir)
        bool isApp = (item.type == "app");
        auto* actionBtn = new QPushButton(isApp ? "Iniciar" : "Reproducir", this);
        actionBtn->setFocusPolicy(Qt::NoFocus);
        m_table->setCellWidget(row, 4, actionBtn);

        const QString ref  = item.ref;
        const QString type = item.type;
        connect(actionBtn, &QPushButton::clicked, this, [this, ref, type]() {
            if (type == "app") {
                m_appManager->start(ref);
            } else {
                if (m_stageWindow && m_stageWindow->isActive()) {
                    if (const auto* e = mediaEntryForId(ref); e && e->type == "video")
                        m_stageWindow->showVideo();
                }
                m_mediaManager->play(ref);
            }
        });

        // Col 5: Stop button
        auto* stopBtn = new QPushButton("Parar", this);
        stopBtn->setFocusPolicy(Qt::NoFocus);
        stopBtn->setEnabled(false);
        m_table->setCellWidget(row, 5, stopBtn);

        connect(stopBtn, &QPushButton::clicked, this, [this, ref, type]() {
            if (type == "app") m_appManager->stop(ref);
            else               m_mediaManager->stop(ref);
        });

        // Col 6: Estado
        auto* stateItem = new QTableWidgetItem("LISTA");
        stateItem->setForeground(CyberTheme::color(CyberTheme::TextMuted));
        m_table->setItem(row, 6, stateItem);

        m_table->setRowHeight(row, 38);
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
    } else {
        MediaState s = m_mediaManager->state(item.ref);
        stateLabel = mediaStateLabel(s);
        stateColor = mediaStateColor(s);
        canAction  = (s == MediaState::Stopped || s == MediaState::Error);
        canStop    = (s == MediaState::Playing);
    }

    if (auto* si = m_table->item(row, 6)) {
        si->setText(stateLabel);
        si->setForeground(stateColor);
    }
    if (auto* b = qobject_cast<QPushButton*>(m_table->cellWidget(row, 4))) b->setEnabled(canAction);
    if (auto* b = qobject_cast<QPushButton*>(m_table->cellWidget(row, 5))) b->setEnabled(canStop);
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

const MediaEntry* RehearsalModeScreen::mediaEntryForId(const QString& id) const {
    for (const auto& e : m_mediaConfig.items())
        if (e.id == id) return &e;
    return nullptr;
}

// ---------- slots ------------------------------------------------------------

void RehearsalModeScreen::onStateChanged(const QString& id, AppState) {
    int row = rowForRef("app", id);
    if (row >= 0) updateRow(row);
}

void RehearsalModeScreen::onMediaStateChanged(const QString& id, MediaState state) {
    int row = rowForRef("media", id);
    if (row >= 0) {
        updateRow(row);
        // Route stage display when a video starts or stops
        if (m_stageWindow && m_stageWindow->isActive()) {
            if (const auto* e = mediaEntryForId(id); e && e->type == "video") {
                if (state == MediaState::Playing)
                    m_stageWindow->showVideo();
                else if (state == MediaState::Stopped || state == MediaState::Error)
                    m_stageWindow->showBlack();
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
        case Qt::Key_Escape:                     emit returnToSelector(); break;
        case Qt::Key_1: case Qt::Key_Left:       emit switchMode(0); break;
        case Qt::Key_3: case Qt::Key_Right:      emit switchMode(2); break;
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
}
