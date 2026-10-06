#ifndef MONITORWORKER_H
#define MONITORWORKER_H

#include <QObject>
#include <QThread>
#include "modbusclient.h"

// 采集工作线程：在子线程里周期性读 Modbus 寄存器，发信号给 UI
// 对应简历关键词：QThread、信号槽、多线程解耦、状态机
class MonitorWorker : public QObject
{
    Q_OBJECT
public:
    explicit MonitorWorker(QObject *parent = nullptr);
    ~MonitorWorker();

public slots:
    void start(const QString &host, quint16 port, quint8 unitId,
               quint16 startAddr, quint16 quantity, int intervalMs);
    void stop();

signals:
    void dataReady(const QVector<quint16> &regs);
    void logMessage(const QString &msg);
    void connectionChanged(bool connected);
    void finished();

private slots:
    void onPollTimeout();

private:
    ModbusClient *m_modbus;
    QTimer *m_timer;
    QTimer *m_reconnectTimer;
    QString m_host;
    quint16 m_port = 502;
    quint8 m_unitId = 1;
    quint16 m_startAddr = 0;
    quint16 m_quantity = 10;
    bool m_running = false;
};

#endif // MONITORWORKER_H
