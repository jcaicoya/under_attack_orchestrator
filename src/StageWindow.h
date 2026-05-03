#pragma once
#include <QWidget>

class QStackedWidget;
class QVideoWidget;
class QKeyEvent;

class StageWindow : public QWidget {
    Q_OBJECT
public:
    enum class Content { Black, Logo, Video };

    explicit StageWindow(QWidget* parent = nullptr);

    void activateOnScreen(int screenIndex);
    void deactivate();

    bool    isActive()          const { return isVisible(); }
    int     activeScreenIndex() const { return m_screenIndex; }
    Content currentContent()    const { return m_content; }
    QVideoWidget* videoOutput() const { return m_videoWidget; }

public slots:
    void showBlack();
    void showLogo();
    void showVideo();
    void softHide();   // hide without emitting deactivated
    void softShow();   // re-show on same screen without emitting activated

signals:
    void activated(int screenIndex);
    void deactivated();
    void contentChanged(StageWindow::Content content);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QStackedWidget* m_stack       = nullptr;
    QVideoWidget*   m_videoWidget = nullptr;
    Content         m_content     = Content::Black;
    int             m_screenIndex = -1;
};
