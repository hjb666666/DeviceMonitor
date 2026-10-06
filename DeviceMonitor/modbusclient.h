#ifndef MODBUSCLIENT_H
#define MODBUSCLIENT_H

#include <QObject>
#include <QtNetwork>

// Modbus TCP 协议封装：读保持寄存器（功能码 0x03）
// 对应简历关键词：Modbus TCP、协议解析、字节序处理
class ModbusClient : public QObject
{
    Q_OBJECT
public:
    explicit ModbusClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    // 读保持寄存器，返回原始数据（大端序寄存器数组），失败返回空
    QVector<quint16> readHoldingRegisters(quint8 unitId, quint16 startAddr, quint16 quantity);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &msg);
    void logMessage(const QString &msg);

private:
    QTcpSocket *m_socket;
    quint16 m_transactionId = 0;
    QByteArray m_rxBuffer;

    QByteArray buildReadRequest(quint8 unitId, quint16 startAddr, quint16 quantity);
    QVector<quint16> parseReadResponse(const QByteArray &frame, quint16 expectedQty);
};

#endif // MODBUSCLIENT_H
