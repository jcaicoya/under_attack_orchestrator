#include "AdbManager.h"
#include <QProcess>
#include <QFileInfo>

static const QString kSdkAdb =
    "C:/Users/caico/AppData/Local/Android/Sdk/platform-tools/adb.exe";

QString AdbManager::adbExe() {
    return QFileInfo::exists(kSdkAdb) ? kSdkAdb : "adb";
}

AdbManager::AdbManager(QObject* parent) : QObject(parent) {}

void AdbManager::connectWifi(const QString& ipPort) {
    runAdb({"connect", ipPort}, "connect " + ipPort,
           [this, ipPort](int, const QString& out) {
               emit log(out.trimmed());
               if (out.contains("connected")) detectDevice();
           });
}

void AdbManager::detectDevice() {
    runAdb({"devices"}, "devices", [this](int, const QString& out) {
        QString found;
        for (const auto& line : out.split('\n')) {
            const QString t = line.trimmed();
            if (t.isEmpty() || t.startsWith("List")) continue;
            if (t.endsWith("device")) {
                found = t.split('\t').first().trimmed();
                break;
            }
        }
        if (!found.isEmpty() && found != m_serial) {
            m_serial = found;
            emit deviceFound(m_serial);
        } else if (found.isEmpty() && !m_serial.isEmpty()) {
            m_serial.clear();
            emit deviceLost();
        } else if (found.isEmpty()) {
            emit log("No device found.");
        }
    });
}

void AdbManager::testConnection() {
    runAdb({"shell", "getprop", "ro.product.model"}, "test",
           [this](int exitCode, const QString& out) {
               const QString model = out.trimmed();
               if (exitCode == 0 && !model.isEmpty()) {
                   emit log(QString("Conexión OK — dispositivo: %1").arg(model));
                   runAdb({"shell", "cmd", "vibrator", "vibrate", "500"}, "vibrate");
                   // Sync label state in case orchestrator was restarted with device already connected.
                   if (m_serial.isEmpty()) detectDevice();
               } else {
                   emit log("Conexión fallida — el dispositivo no responde.");
                   if (!m_serial.isEmpty()) { m_serial.clear(); emit deviceLost(); }
               }
           });
}

void AdbManager::disconnectDevice() {
    if (m_serial.isEmpty()) return;
    const QString serial = m_serial;
    m_serial.clear();
    emit deviceLost();
    // For WiFi connections, also tell adb to drop the TCP link.
    if (serial.contains(':'))
        runAdb({"disconnect", serial}, "disconnect " + serial);
    else
        emit log("USB device — unplug the cable to fully disconnect.");
}

void AdbManager::setupReverseTunnel(quint16 port) {
    const QString p = QString::number(port);
    runAdb({"reverse", "tcp:" + p, "tcp:" + p}, "reverse tcp:" + p);
}

void AdbManager::launchApp(const QString& package, const QString& activity) {
    runAdb({"shell", "am", "start", "-n", package + "/" + activity},
           "launch " + package);
}

void AdbManager::stopApp(const QString& package) {
    runAdb({"shell", "am", "force-stop", package}, "stop " + package);
}

void AdbManager::runAdb(const QStringList& args, const QString& label,
                         std::function<void(int, const QString&)> onDone) {
    auto* proc = new QProcess(this);
    connect(proc, &QProcess::finished, this,
            [this, proc, label, onDone](int exitCode, QProcess::ExitStatus) {
                const QString out = proc->readAllStandardOutput();
                const QString err = proc->readAllStandardError();
                emit log(QString("[adb %1] exit=%2").arg(label).arg(exitCode));
                if (!out.trimmed().isEmpty()) emit log(out.trimmed());
                if (!err.trimmed().isEmpty()) emit log("ERR: " + err.trimmed());
                if (onDone) onDone(exitCode, out);
                proc->deleteLater();
            });
    proc->start(adbExe(), args);
    if (!proc->waitForStarted(2000)) {
        emit log("ERROR: adb not found. Expected at " + kSdkAdb);
        proc->deleteLater();
    }
}
