#pragma once

#include <QMetaType>
#include <QSerialPort>
#include <QString>

/// 串口连接参数（可在信号槽中跨线程传递）
struct SerialConfig {
    QString portName;
    qint32 baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;

    bool operator==(const SerialConfig &other) const
    {
        return portName == other.portName && baudRate == other.baudRate
            && dataBits == other.dataBits && parity == other.parity
            && stopBits == other.stopBits && flowControl == other.flowControl;
    }
    bool operator!=(const SerialConfig &other) const { return !(*this == other); }
};

Q_DECLARE_METATYPE(SerialConfig)
