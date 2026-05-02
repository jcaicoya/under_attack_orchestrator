#include <QApplication>
#include "Logger.h"
#include "MainWindow.h"
#include "CyberTheme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Cybershow Orchestrator");
    app.setOrganizationName("Cybershow");
    app.setStyleSheet(CyberTheme::globalStyleSheet());

    QString packageRoot = QCoreApplication::applicationDirPath();

    Logger::instance().init(packageRoot + "/logs");
    Logger::instance().log("Cybershow Orchestrator started.");
    Logger::instance().log(QString("Package root: %1").arg(packageRoot));

    MainWindow window(packageRoot);
    window.show();

    return app.exec();
}
