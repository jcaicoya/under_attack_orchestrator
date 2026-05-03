#include "StageWindow.h"
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QKeyEvent>

StageWindow::StageWindow(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
{
    setWindowTitle("Cybershow — Escenario");
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new QStackedWidget(this);
    root->addWidget(m_stack);

    // Index 0: Black screen
    auto* blackPanel = new QWidget(m_stack);
    blackPanel->setStyleSheet("background: black;");
    m_stack->addWidget(blackPanel);

    // Index 1: Logo / waiting screen
    auto* logoPanel = new QWidget(m_stack);
    logoPanel->setStyleSheet("background: black;");
    auto* logoLay = new QVBoxLayout(logoPanel);
    logoLay->setAlignment(Qt::AlignCenter);
    auto* titleLabel = new QLabel("BAJO ATAQUE", logoPanel);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "QLabel { color: #FF2020; font-size: 80px; font-weight: 900;"
        " letter-spacing: 12px; background: transparent; }");
    auto* subLabel = new QLabel("en vivo", logoPanel);
    subLabel->setAlignment(Qt::AlignCenter);
    subLabel->setStyleSheet(
        "QLabel { color: #3A1010; font-size: 22px; font-weight: 400;"
        " letter-spacing: 8px; background: transparent; }");
    logoLay->addWidget(titleLabel);
    logoLay->addSpacing(12);
    logoLay->addWidget(subLabel);
    m_stack->addWidget(logoPanel);

    m_stack->setCurrentIndex(0);
}

// Force the native window onto the given screen before showing.
// On Windows, showFullScreen() on a never-shown window always lands on the
// primary monitor regardless of setGeometry(), unless the screen is set first.
static void pinToScreen(QWidget* w, QScreen* screen) {
    w->winId(); // forces native window creation; windowHandle() is valid after this
    if (auto* wh = w->windowHandle())
        wh->setScreen(screen);
}

void StageWindow::activateOnScreen(int screenIndex) {
    const auto screens = QGuiApplication::screens();
    if (screenIndex < 0 || screenIndex >= screens.size()) return;
    QScreen* screen = screens[screenIndex];
    pinToScreen(this, screen);
    setGeometry(screen->geometry());
    showFullScreen();
    raise();
    m_screenIndex = screenIndex;
    emit activated(screenIndex);
}

void StageWindow::deactivate() {
    hide();
    m_screenIndex = -1;
    emit deactivated();
}

void StageWindow::showBlack() {
    m_stack->setCurrentIndex(0);
    m_content = Content::Black;
    raise();
    emit contentChanged(m_content);
}

void StageWindow::showLogo() {
    m_stack->setCurrentIndex(1);
    m_content = Content::Logo;
    raise();
    emit contentChanged(m_content);
}

void StageWindow::softHide() {
    hide();
}

void StageWindow::softShow() {
    if (m_screenIndex < 0) return;
    const auto screens = QGuiApplication::screens();
    if (m_screenIndex >= screens.size()) return;
    QScreen* screen = screens[m_screenIndex];
    pinToScreen(this, screen);
    setGeometry(screen->geometry());
    showFullScreen();
    raise();
}

void StageWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape)
        deactivate();
    else
        QWidget::keyPressEvent(event);
}
