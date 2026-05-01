#pragma once

#include <QWidget>
#include <QVector>

class ModeCard;

class ModeSelectorScreen : public QWidget {
    Q_OBJECT
public:
    explicit ModeSelectorScreen(QWidget* parent = nullptr);

signals:
    void modeConfirmed(int modeIndex);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void selectCard(int index);
    void confirmMode(int index);

    QVector<ModeCard*> m_cards;
    int                m_selectedIndex = 0;
};
