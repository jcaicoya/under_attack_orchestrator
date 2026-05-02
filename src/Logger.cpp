#include "Logger.h"
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(const QString& logDir) {
    QDir().mkpath(logDir);
    QString filename = logDir + "/cybershow_" +
        QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss") + ".log";
    m_file.setFileName(filename);
    if (m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
        m_initialized = true;
    else
        qWarning() << "Logger: could not open" << filename;
}

void Logger::log(const QString& message) {
    QString formatted = QString("[%1] %2")
        .arg(QTime::currentTime().toString("hh:mm:ss"), message);

    if (m_initialized) {
        QTextStream out(&m_file);
        out << formatted << "\n";
        m_file.flush();
    }

    emit messageLogged(formatted);
}
