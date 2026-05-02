#pragma once
#include <QObject>
#include <QFile>

class Logger : public QObject {
    Q_OBJECT
public:
    static Logger& instance();
    void init(const QString& logDir);

public slots:
    void log(const QString& message);

signals:
    void messageLogged(const QString& formatted);

private:
    Logger() = default;
    QFile m_file;
    bool  m_initialized = false;
};
