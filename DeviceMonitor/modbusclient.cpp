#include "modbusclient.h"

ModbusClient::ModbusClient(QObject *parent)
    : QObject(parent)
{
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, [this](){ emit connected(); });
    connect(m_socket, &QTcpSocket::disconnected, this, [this](){ emit disconnected(); });
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, [this](QAbstractSocket::SocketError){
        emit errorOccurred(m_socket->errorString());
    });
}

void ModbusClient::connectToServer(const QString &host, quint16 port)
{
    m_socket->abort();
    m_rxBuffer.clear();
    m_socket->connectToHost(host, port);
}

void ModbusClient::disconnectFromServer()
{
    m_socket->disconnectFromHost();
}

bool ModbusClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

QByteArray ModbusClient::buildReadRequest(quint8 unitId, quint16 startAddr, quint16 quantity)
{
    // MBAP Header: TransactionID(2) + ProtocolID(2)=0 + Length(2) + UnitID(1)
    // PDU: FunctionCode(1)=0x03 + StartAddr(2) + Quantity(2)
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);

    m_transactionId++;
    ds << quint16(m_transactionId);  // Transaction ID
    ds << quint16(0);                // Protocol ID
    ds << quint16(6);                // Length: UnitID(1)+Func(1)+Addr(2)+Qty(2)
    ds << unitId;                    // Unit ID
    ds << quint8(0x03);              // Function code
    ds << startAddr;                 // Starting address
    ds << quantity;                  // Quantity
    return frame;
}

QVector<quint16> ModbusClient::parseReadResponse(const QByteArray &frame, quint16 expectedQty)
{
    QVector<quint16> result;
    // 最小响应: MBAP(7) + Func(1) + ByteCount(1) = 9
    if (frame.size() < 9) return result;

    QDataStream ds(frame);
    ds.setByteOrder(QDataStream::BigEndian);

    quint16 tid, pid, len;
    quint8 unitId, func, byteCount;
    ds >> tid >> pid >> len >> unitId >> func >> byteCount;

    if (func & 0x80) {
        emit errorOccurred(QString("Modbus 异常码: 0x%1").arg(func, 2, 16, QChar('0')));
        return result;
    }

    if (byteCount != expectedQty * 2) {
        emit errorOccurred(QString("字节数不匹配: 期望 %1, 实际 %2").arg(expectedQty*2).arg(byteCount));
        return result;
    }

    for (int i = 0; i < expectedQty; i++) {
        quint16 reg;
        ds >> reg;
        result.append(reg);
    }
    return result;
}

QVector<quint16> ModbusClient::readHoldingRegisters(quint8 unitId, quint16 startAddr, quint16 quantity)
{
    if (!isConnected()) return {};

    m_socket->write(buildReadRequest(unitId, startAddr, quantity));
    m_socket->flush();

    // 同步等待响应（超时 1000ms）
    if (!m_socket->waitForReadyRead(1000)) {
        emit errorOccurred("等待响应超时");
        return {};
    }

    m_rxBuffer += m_socket->readAll();

    // 收齐完整帧：MBAP Length 字段
    if (m_rxBuffer.size() < 6) return {};
    QDataStream ds(m_rxBuffer);
    ds.setByteOrder(QDataStream::BigEndian);
    quint16 tid, pid, len;
    ds >> tid >> pid >> len;
    int totalLen = 6 + len;  // MBAP 前6字节 + 后续长度
    if (m_rxBuffer.size() < totalLen) return {};

    QVector<quint16> result = parseReadResponse(m_rxBuffer.left(totalLen), quantity);
    m_rxBuffer.remove(0, totalLen);
    return result;
}
