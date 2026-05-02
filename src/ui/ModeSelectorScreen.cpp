#include "ModeSelectorScreen.h"
#include "CyberTheme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QShowEvent>

// ---------------------------------------------------------------------------
// ModeCard
// ---------------------------------------------------------------------------

class ModeCard : public QFrame {
    Q_OBJECT
public:
    ModeCard(const QString& title,
             const QString& description,
             bool           available,
             QWidget*       parent = nullptr)
        : QFrame(parent), m_available(available)
    {
        setFixedSize(230, 270);
        setFocusPolicy(Qt::NoFocus);
        setCursor(available ? Qt::PointingHandCursor : Qt::ArrowCursor);

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 28, 24, 24);
        root->setSpacing(0);

        // Number badge
        m_numberLabel = new QLabel(this);
        m_numberLabel->setAlignment(Qt::AlignLeft);
        root->addWidget(m_numberLabel);

        root->addSpacing(16);

        // Mode title
        m_titleLabel = new QLabel(title, this);
        m_titleLabel->setWordWrap(false);
        m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        root->addWidget(m_titleLabel);

        root->addSpacing(12);

        // Description
        m_descLabel = new QLabel(description, this);
        m_descLabel->setWordWrap(true);
        m_descLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        root->addWidget(m_descLabel, 1);

        root->addSpacing(16);

        // Status
        m_statusLabel = new QLabel(this);
        m_statusLabel->setAlignment(Qt::AlignLeft);
        root->addWidget(m_statusLabel);

        applyStyle(false);
    }

    void setNumber(int n) {
        m_numberLabel->setText(QString::number(n));
    }

    void setSelected(bool selected) {
        if (m_selected == selected) return;
        m_selected = selected;
        applyStyle(selected);
    }

    bool isAvailable() const { return m_available; }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton)
            emit clicked();
    }

private:
    void applyStyle(bool selected) {
        QString borderColor;
        QString bgColor;
        QString titleColor;
        QString descColor;
        QString numColor;
        QString statusText;
        QString statusColor;

        if (!m_available) {
            bgColor     = "#0C0F14";
            borderColor = "#1A2030";
            titleColor  = "#3A4455";
            descColor   = "#2A3040";
            numColor    = "#2A3040";
            statusText  = "○  Próximamente";
            statusColor = "#3A4455";
        } else if (selected) {
            bgColor     = "#111828";
            borderColor = CyberTheme::AccentPrimary;
            titleColor  = CyberTheme::TextPrimary;
            descColor   = CyberTheme::TextSecondary;
            numColor    = CyberTheme::AccentPrimary;
            statusText  = "●  Disponible";
            statusColor = CyberTheme::AccentGreen;
        } else {
            bgColor     = "#101318";
            borderColor = "#293241";
            titleColor  = CyberTheme::TextPrimary;
            descColor   = CyberTheme::TextSecondary;
            numColor    = "#5F6B78";
            statusText  = "●  Disponible";
            statusColor = CyberTheme::AccentGreenSoft;
        }

        setStyleSheet(QString(
            "ModeCard {"
            "  background-color: %1;"
            "  border: 2px solid %2;"
            "  border-radius: 18px;"
            "}"
        ).arg(bgColor, borderColor));

        auto applyLabel = [](QLabel* l, const QString& color, int size, bool bold) {
            l->setStyleSheet(QString(
                "QLabel { color: %1; font-size: %2px; font-weight: %3; background: transparent; }"
            ).arg(color).arg(size).arg(bold ? "900" : "400"));
        };

        applyLabel(m_numberLabel, numColor,  13, true);
        applyLabel(m_titleLabel,  titleColor, 20, true);
        applyLabel(m_descLabel,   descColor,  12, false);

        m_statusLabel->setStyleSheet(QString(
            "QLabel { color: %1; font-size: 11px; font-weight: 700; background: transparent; }"
        ).arg(statusColor));
        m_statusLabel->setText(statusText);
    }

    bool    m_available;
    bool    m_selected = false;
    QLabel* m_numberLabel = nullptr;
    QLabel* m_titleLabel  = nullptr;
    QLabel* m_descLabel   = nullptr;
    QLabel* m_statusLabel = nullptr;
};

#include "ModeSelectorScreen.moc"

// ---------------------------------------------------------------------------
// ModeSelectorScreen
// ---------------------------------------------------------------------------

ModeSelectorScreen::ModeSelectorScreen(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 0, 40, 40);
    root->setSpacing(0);

    root->addStretch(2);

    // Title block
    auto* titleBlock = new QVBoxLayout();
    titleBlock->setSpacing(6);
    titleBlock->setAlignment(Qt::AlignHCenter);

    auto* kicker = new QLabel("CYBERSHOW", this);
    kicker->setObjectName("SceneTitle");
    kicker->setAlignment(Qt::AlignHCenter);
    titleBlock->addWidget(kicker);

    auto* subtitle = new QLabel("Centro de control", this);
    subtitle->setObjectName("ScreenSubtitle");
    subtitle->setAlignment(Qt::AlignHCenter);
    titleBlock->addWidget(subtitle);

    root->addLayout(titleBlock);
    root->addStretch(1);

    // Cards row
    auto* cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(24);
    cardsRow->setAlignment(Qt::AlignHCenter);

    struct CardDef { QString title; QString description; bool available; };
    const QVector<CardDef> defs = {
        { "CONFIGURAR", "Preparar aplicaciones,\najustar rutas y\nprobar conexiones.",   true  },
        { "ENSAYO",     "Ensayo técnico sin\npantallas de\nconfiguración.",               true  },
        { "SHOW",       "Modo seguro para\nla actuación real.",                           true  },
    };

    for (int i = 0; i < defs.size(); ++i) {
        const auto& d = defs[i];
        auto* card = new ModeCard(d.title, d.description, d.available, this);
        card->setNumber(i + 1);
        m_cards.append(card);
        cardsRow->addWidget(card);

        connect(card, &ModeCard::clicked, this, [this, i]() {
            selectCard(i);
            if (m_cards[i]->isAvailable())
                confirmMode(i);
        });
    }

    root->addLayout(cardsRow);
    root->addStretch(2);

    // Keyboard hint
    auto* hint = new QLabel("1 · Configurar     2 · Ensayo     3 · Show     ←  →  Navegar     Enter · Abrir", this);
    hint->setAlignment(Qt::AlignHCenter);
    hint->setStyleSheet(
        "QLabel { color: #3A4455; font-size: 11px; font-weight: 600; background: transparent; }");
    root->addWidget(hint);

    // Initial selection
    m_cards[0]->setSelected(true);
}

void ModeSelectorScreen::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    setFocus();
}

void ModeSelectorScreen::selectCard(int index) {
    if (index < 0 || index >= m_cards.size()) return;
    m_cards[m_selectedIndex]->setSelected(false);
    m_selectedIndex = index;
    m_cards[m_selectedIndex]->setSelected(true);
}

void ModeSelectorScreen::confirmMode(int index) {
    emit modeConfirmed(index);
}

void ModeSelectorScreen::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_1:
            selectCard(0);
            if (m_cards[0]->isAvailable()) confirmMode(0);
            break;
        case Qt::Key_2:
            selectCard(1);
            if (m_cards[1]->isAvailable()) confirmMode(1);
            break;
        case Qt::Key_3:
            selectCard(2);
            if (m_cards[2]->isAvailable()) confirmMode(2);
            break;
        case Qt::Key_Left:
            selectCard(qMax(0, m_selectedIndex - 1));
            break;
        case Qt::Key_Right:
            selectCard(qMin(m_cards.size() - 1, m_selectedIndex + 1));
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            if (m_cards[m_selectedIndex]->isAvailable())
                confirmMode(m_selectedIndex);
            break;
        default:
            QWidget::keyPressEvent(event);
    }
}
