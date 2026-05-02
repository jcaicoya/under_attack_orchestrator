#include "CyberBackgroundWidget.h"
#include "CyberTheme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QPen>
#include <algorithm>

CyberBackgroundWidget::CyberBackgroundWidget(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
}

void CyberBackgroundWidget::setGridVisible(bool visible) {
    if (m_gridVisible == visible) {
        return;
    }
    m_gridVisible = visible;
    update();
}

void CyberBackgroundWidget::setCircuitDetailsVisible(bool visible) {
    if (m_circuitDetailsVisible == visible) {
        return;
    }
    m_circuitDetailsVisible = visible;
    update();
}

void CyberBackgroundWidget::setGlowIntensity(qreal intensity) {
    const qreal clamped = std::clamp(intensity, 0.0, 2.0);
    if (qFuzzyCompare(m_glowIntensity, clamped)) {
        return;
    }
    m_glowIntensity = clamped;
    update();
}

void CyberBackgroundWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();
    paintBase(painter, r);
    paintGlows(painter, r);

    if (m_gridVisible) {
        paintGrid(painter, r);
    }

    if (m_circuitDetailsVisible) {
        paintCircuitDetails(painter, r);
    }

    paintVignette(painter, r);
}

void CyberBackgroundWidget::paintBase(QPainter& painter, const QRect& r) {
    painter.fillRect(r, CyberTheme::color(CyberTheme::BackgroundBase));

    QLinearGradient gradient(r.topLeft(), r.bottomRight());
    gradient.setColorAt(0.0, CyberTheme::color(CyberTheme::BackgroundBase));
    gradient.setColorAt(0.55, CyberTheme::color(CyberTheme::BackgroundSoft));
    gradient.setColorAt(1.0, CyberTheme::color(CyberTheme::BackgroundBlueDepth));
    painter.fillRect(r, gradient);
}

void CyberBackgroundWidget::paintGlows(QPainter& painter, const QRect& r) {
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Screen);

    const int w = r.width();
    const int h = r.height();

    auto alpha = [this](int base) {
        return std::clamp(static_cast<int>(base * m_glowIntensity), 0, 255);
    };

    QRadialGradient topRight(QPointF(w * 0.82, h * 0.18), std::max(w, h) * 0.55);
    topRight.setColorAt(0.0, CyberTheme::color(CyberTheme::AccentCyan, alpha(30)));
    topRight.setColorAt(0.35, CyberTheme::color(CyberTheme::AccentPrimary, alpha(12)));
    topRight.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(r, topRight);

    QRadialGradient bottomLeft(QPointF(w * 0.12, h * 0.88), std::max(w, h) * 0.50);
    bottomLeft.setColorAt(0.0, CyberTheme::color(CyberTheme::AccentGreen, alpha(20)));
    bottomLeft.setColorAt(0.45, CyberTheme::color(CyberTheme::AccentPrimary, alpha(8)));
    bottomLeft.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(r, bottomLeft);

    painter.restore();
}

void CyberBackgroundWidget::paintGrid(QPainter& painter, const QRect& r) {
    painter.save();

    constexpr int spacing = 64;
    QPen minorPen(CyberTheme::color("#17324A", 24));
    minorPen.setWidthF(1.0);
    painter.setPen(minorPen);

    const int startX = r.left() - (r.left() % spacing);
    const int startY = r.top() - (r.top() % spacing);

    for (int x = startX; x <= r.right(); x += spacing) {
        painter.drawLine(QPoint(x, r.top()), QPoint(x, r.bottom()));
    }

    for (int y = startY; y <= r.bottom(); y += spacing) {
        painter.drawLine(QPoint(r.left(), y), QPoint(r.right(), y));
    }

    // Slightly stronger peripheral guide lines. They frame the screen without
    // adding visual weight behind the central setup card.
    QPen peripheralPen(CyberTheme::color("#17324A", 38));
    peripheralPen.setWidthF(1.0);
    painter.setPen(peripheralPen);

    const int margin = 48;
    painter.drawRect(r.adjusted(margin, margin, -margin, -margin));

    painter.restore();
}

void CyberBackgroundWidget::paintCircuitDetails(QPainter& painter, const QRect& r) {
    painter.save();

    QPen pen(CyberTheme::color("#1C2E40", 70));
    pen.setWidthF(1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const int w = r.width();
    const int h = r.height();

    // Top-left technical corner
    QPainterPath tl;
    tl.moveTo(72, 128);
    tl.lineTo(150, 128);
    tl.lineTo(150, 92);
    tl.lineTo(230, 92);
    painter.drawPath(tl);
    painter.drawEllipse(QPointF(230, 92), 3.0, 3.0);
    painter.drawEllipse(QPointF(150, 128), 2.5, 2.5);

    // Bottom-right technical corner
    QPainterPath br;
    br.moveTo(w - 80, h - 140);
    br.lineTo(w - 170, h - 140);
    br.lineTo(w - 170, h - 96);
    br.lineTo(w - 260, h - 96);
    painter.drawPath(br);
    painter.drawEllipse(QPointF(w - 260, h - 96), 3.0, 3.0);
    painter.drawEllipse(QPointF(w - 170, h - 140), 2.5, 2.5);

    // Right side small fragments
    QPen accentPen(CyberTheme::color(CyberTheme::AccentCyan, 45));
    accentPen.setWidthF(1.2);
    painter.setPen(accentPen);

    painter.drawLine(QPointF(w - 210, h * 0.28), QPointF(w - 130, h * 0.28));
    painter.drawLine(QPointF(w - 130, h * 0.28), QPointF(w - 130, h * 0.34));
    painter.drawEllipse(QPointF(w - 130, h * 0.34), 2.0, 2.0);

    painter.drawLine(QPointF(110, h * 0.72), QPointF(190, h * 0.72));
    painter.drawLine(QPointF(190, h * 0.72), QPointF(190, h * 0.78));
    painter.drawEllipse(QPointF(190, h * 0.78), 2.0, 2.0);

    painter.restore();
}

void CyberBackgroundWidget::paintVignette(QPainter& painter, const QRect& r) {
    painter.save();

    const QPointF center = r.center();
    const qreal radius = std::max(r.width(), r.height()) * 0.72;

    QRadialGradient vignette(center, radius);
    vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
    vignette.setColorAt(0.58, QColor(0, 0, 0, 15));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 150));

    painter.fillRect(r, vignette);
    painter.restore();
}
