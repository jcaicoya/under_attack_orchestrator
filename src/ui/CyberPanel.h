#pragma once

#include <QFrame>
#include <QString>

class QVBoxLayout;
class QLabel;

class CyberPanel : public QFrame {
    Q_OBJECT

public:
    enum class Variant {
        Normal,
        Raised,
        Critical
    };

    explicit CyberPanel(const QString& title = QString(), QWidget* parent = nullptr);

    QVBoxLayout* bodyLayout() const { return m_bodyLayout; }
    void setTitle(const QString& title);
    void setVariant(Variant variant);

private:
    QLabel* m_titleLabel = nullptr;
    QVBoxLayout* m_rootLayout = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;
};
