#include "CyberTheme.h"

namespace CyberTheme {

QColor color(const char* hex, int alpha) {
    QColor c(QString::fromUtf8(hex));
    c.setAlpha(alpha);
    return c;
}

QString globalStyleSheet() {
    return QStringLiteral(R"QSS(
        QWidget {
            background: transparent;
            color: #F2F5F8;
            font-family: "Segoe UI", "Inter", "Arial";
            font-size: 13px;
        }

        QLabel#ScreenTitle {
            color: #F2F5F8;
            font-size: 24px;
            font-weight: 800;
            letter-spacing: 0.8px;
        }

        QLabel#SceneTitle {
            color: #F2F5F8;
            font-size: 42px;
            font-weight: 900;
            letter-spacing: 3px;
        }

        QLabel#ScreenSubtitle {
            color: #8D96A3;
            font-size: 13px;
            font-weight: 600;
            letter-spacing: 0.5px;
        }

        QLabel#KickerLabel {
            color: #00D1FF;
            font-size: 11px;
            font-weight: 800;
            letter-spacing: 2px;
        }

        QFrame#CyberPanel,
        QWidget#CyberPanel {
            background-color: #101318;
            border: 1px solid #293241;
            border-radius: 12px;
        }

        QFrame#CyberPanelRaised,
        QWidget#CyberPanelRaised {
            background-color: #151922;
            border: 1px solid #334052;
            border-radius: 14px;
        }

        QFrame#CyberPanelCritical,
        QWidget#CyberPanelCritical {
            background-color: rgba(70, 0, 0, 0.50);
            border: 2px solid #FF3347;
            border-radius: 12px;
        }

        QLabel#PanelTitle {
            color: #F2F5F8;
            font-size: 16px;
            font-weight: 800;
        }

        QLabel#FieldLabel {
            color: #B8C0CC;
            font-size: 12px;
            font-weight: 600;
        }

        QLabel#MutedLabel {
            color: #8D96A3;
            font-size: 11px;
        }

        QLabel#StatusOk {
            color: #00FF55;
            font-family: "Consolas", "JetBrains Mono", monospace;
            font-size: 12px;
            font-weight: 700;
        }

        QLabel#StatusInfo {
            color: #00D1FF;
            font-family: "Consolas", "JetBrains Mono", monospace;
            font-size: 12px;
            font-weight: 700;
        }

        QLabel#StatusWarning {
            color: #FFB000;
            font-family: "Consolas", "JetBrains Mono", monospace;
            font-size: 12px;
            font-weight: 700;
        }

        QLabel#StatusError {
            color: #FF3347;
            font-family: "Consolas", "JetBrains Mono", monospace;
            font-size: 12px;
            font-weight: 800;
        }

        QLabel#MetricValue {
            font-size: 58px;
            font-weight: 900;
            letter-spacing: 2px;
        }

        QLabel#MetricLabel {
            color: #8D96A3;
            font-size: 12px;
            font-weight: 800;
            letter-spacing: 2px;
        }

        QLineEdit, QSpinBox, QComboBox, QTextEdit, QListWidget, QPlainTextEdit {
            background-color: #20242B;
            color: #F2F5F8;
            border: 1px solid #3B4654;
            border-radius: 7px;
            padding: 8px 10px;
            selection-background-color: #1688E8;
        }

        QLineEdit:focus, QSpinBox:focus, QComboBox:focus, QTextEdit:focus, QListWidget:focus, QPlainTextEdit:focus {
            border: 1px solid #2EA8FF;
        }

        QPushButton {
            background-color: #202633;
            color: #F2F5F8;
            border: 1px solid #3B4654;
            border-radius: 8px;
            padding: 8px 14px;
            font-weight: 700;
        }

        QPushButton:hover {
            background-color: #263044;
            border-color: #64748B;
        }

        QPushButton:focus {
            outline: none;
        }

        QPushButton:disabled {
            color: #5F6B78;
            background-color: #151922;
            border-color: #293241;
        }

        QPushButton#PrimaryButton {
            background-color: #1688E8;
            color: white;
            border: 1px solid #2EA8FF;
            border-radius: 9px;
            padding: 12px 18px;
            font-size: 14px;
            font-weight: 900;
            letter-spacing: 0.7px;
        }

        QPushButton#PrimaryButton:hover {
            background-color: #2EA8FF;
        }

        QPushButton#SecondaryButton {
            background-color: #10251F;
            color: #F2F5F8;
            border: 1px solid #00C78C;
        }

        QPushButton#DangerButton {
            background-color: #3A1116;
            color: #FFE8E8;
            border: 1px solid #FF3347;
        }

        QPushButton#NavButton {
            background-color: #141820;
            color: #8D96A3;
            border: 1px solid #293241;
            border-radius: 7px;
            padding: 8px 12px;
            font-size: 12px;
            font-weight: 800;
        }

        QPushButton#NavButton:checked {
            background-color: #202633;
            color: #F2F5F8;
            border: 1px solid #2EA8FF;
        }

        QPushButton#NavButton:hover {
            color: #F2F5F8;
            border-color: #64748B;
        }
    )QSS");
}

} // namespace CyberTheme
