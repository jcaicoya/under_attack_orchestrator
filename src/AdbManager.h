#pragma once
#include <QObject>
#include <functional>

class QProcess;

class AdbManager : public QObject {
    Q_OBJECT
public:
    explicit AdbManager(QObject* parent = nullptr);

    void connectWifi(const QString& ipPort);
    void detectDevice();
    void disconnectDevice();
    void testConnection();
    void setupReverseTunnel(quint16 port);
    void launchApp(const QString& package, const QString& activity);
    void bringAppToFront(const QString& package, const QString& activity);
    void sendAppToBackground();
    void stopApp(const QString& package);

    bool    hasDevice() const { return !m_serial.isEmpty(); }
    QString serial()    const { return m_serial; }

signals:
    void deviceFound(const QString& serial);
    void deviceLost();
    void log(const QString& text);

private:
    void runAdb(const QStringList& args, const QString& label,
                std::function<void(int exitCode, const QString& out)> onDone = nullptr);
    static QString adbExe();

    QString m_serial;
};
