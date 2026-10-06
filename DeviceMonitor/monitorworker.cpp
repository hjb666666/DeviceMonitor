#include "monitorworker.h"
#include <QTimer>

MonitorWorker::MonitorWorker(QObject *parent)
    : QObject(parent)
{
    m_modbus = new ModbusClient(this);
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(3000);  // 3秒后重连

    connect(m_timer, &QTimer::timeout, this, &MonitorWorker::onPollTimeout);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this](){
        if (m_running) {
            emit logMessage("尝试重新连接...");
            m_modbus->connectToServer(m_host, m_port);
        }
    });
    connect(m_modbus, &ModbusClient::connected, this, [this](){
        emit connectionChanged(true);
        emit logMessage("已连接到设备");
        m_reconnectTimer->stop();
    });
    connect(m_modbus, &ModbusClient::disconnected, this, [this](){
        emit connectionChanged(false);
        emit logMessage("连接断开");
        if (m_running) m_reconnectTimer->start();  // 自动重连
    });
    connect(m_modbus, &ModbusClient::errorOccurred, this, [this](const QString &msg){
        emit logMessage("[错误] " + msg);
    });
}

MonitorWorker::~MonitorWorker()
{
    stop();
}

void MonitorWorker::start(const QString &host, quint16 port, quint8 unitId,
                          quint16 startAddr, quint16 quantity, int intervalMs)
{
    m_host = host;
    m_port = port;
    m_unitId = unitId;
    m_startAddr = startAddr;
    m_quantity = quantity;
    m_running = true;

    emit logMessage(QString("正在连接 %1:%2 ...").arg(host).arg(port));
    m_modbus->connectToServer(host, port);

    m_timer->start(intervalMs);
}

void MonitorWorker::stop()
{
    m_running = false;
    m_timer->stop();
    m_modbus->disconnectFromServer();
}

void MonitorWorker::onPollTimeout()
{
    if (!m_running) return;
    QVector<quint16> regs = m_modbus->readHoldingRegisters(m_unitId, m_startAddr, m_quantity);
    if (!regs.isEmpty()) {
        emit dataReady(regs);
    }
}
