#pragma once

#include <QWidget>

class CyberBackgroundWidget : public QWidget {
    Q_OBJECT

public:
    explicit CyberBackgroundWidget(QWidget* parent = nullptr);

    void setGridVisible(bool visible);
    void setCircuitDetailsVisible(bool visible);
    void setGlowIntensity(qreal intensity);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_gridVisible = true;
    bool m_circuitDetailsVisible = true;
    qreal m_glowIntensity = 1.0;

    void paintBase(QPainter& painter, const QRect& r);
    void paintGlows(QPainter& painter, const QRect& r);
    void paintGrid(QPainter& painter, const QRect& r);
    void paintCircuitDetails(QPainter& painter, const QRect& r);
    void paintVignette(QPainter& painter, const QRect& r);
};
