#include "CyberPanel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QStyle>

CyberPanel::CyberPanel(const QString& title, QWidget* parent) : QFrame(parent) {
    setObjectName("CyberPanel");

    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(18, 16, 18, 18);
    m_rootLayout->setSpacing(10);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName("PanelTitle");
    m_titleLabel->setVisible(!title.isEmpty());
    m_rootLayout->addWidget(m_titleLabel);

    m_bodyLayout = new QVBoxLayout();
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(8);
    m_rootLayout->addLayout(m_bodyLayout, 1);
}

void CyberPanel::setTitle(const QString& title) {
    m_titleLabel->setText(title);
    m_titleLabel->setVisible(!title.isEmpty());
}

void CyberPanel::setVariant(Variant variant) {
    switch (variant) {
    case Variant::Normal:
        setObjectName("CyberPanel");
        break;
    case Variant::Raised:
        setObjectName("CyberPanelRaised");
        break;
    case Variant::Critical:
        setObjectName("CyberPanelCritical");
        break;
    }
    style()->unpolish(this);
    style()->polish(this);
}
