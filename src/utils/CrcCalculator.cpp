#include "utils/CrcCalculator.h"

quint8 CrcCalculator::crc8(const quint8 *data, int length)
{
    quint8 crc = 0x00;
    for (int i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80) ? static_cast<quint8>((crc << 1) ^ 0x07)
                               : static_cast<quint8>(crc << 1);
    }
    return crc;
}

quint8 CrcCalculator::crc8(const QByteArray &data)
{
    return crc8(reinterpret_cast<const quint8 *>(data.constData()), data.size());
}

quint16 CrcCalculator::crc16Modbus(const quint8 *data, int length)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x0001) ? static_cast<quint16>((crc >> 1) ^ 0xA001)
                                 : static_cast<quint16>(crc >> 1);
    }
    return crc;
}

quint16 CrcCalculator::crc16Modbus(const QByteArray &data)
{
    return crc16Modbus(reinterpret_cast<const quint8 *>(data.constData()), data.size());
}

quint16 CrcCalculator::crc16CcittFalse(const quint8 *data, int length)
{
    quint16 crc = 0xFFFF;
    for (int i = 0; i < length; ++i) {
        crc ^= static_cast<quint16>(data[i] << 8);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 0x8000) ? static_cast<quint16>((crc << 1) ^ 0x1021)
                                 : static_cast<quint16>(crc << 1);
    }
    return crc;
}

quint16 CrcCalculator::crc16CcittFalse(const QByteArray &data)
{
    return crc16CcittFalse(reinterpret_cast<const quint8 *>(data.constData()), data.size());
}

quint32 CrcCalculator::crc32(const quint8 *data, int length)
{
    quint32 crc = 0xFFFFFFFFu;
    for (int i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFu;
}

quint32 CrcCalculator::crc32(const QByteArray &data)
{
    return crc32(reinterpret_cast<const quint8 *>(data.constData()), data.size());
}

quint8 CrcCalculator::sum8(const QByteArray &data)
{
    quint32 sum = 0;
    for (const char c : data)
        sum += static_cast<quint8>(c);
    return static_cast<quint8>(sum & 0xFF);
}

quint8 CrcCalculator::xor8(const QByteArray &data)
{
    quint8 value = 0;
    for (const char c : data)
        value ^= static_cast<quint8>(c);
    return value;
}

quint8 CrcCalculator::lrc8(const QByteArray &data)
{
    quint32 sum = 0;
    for (const char c : data)
        sum += static_cast<quint8>(c);
    // Modbus ASCII 的 LRC：所有字节求和后取二进制补码
    return static_cast<quint8>((~sum + 1) & 0xFF);
}

QByteArray CrcCalculator::appendChecksum(const QByteArray &payload, AppendMode mode)
{
    QByteArray out = payload;
    switch (mode) {
    case AppendMode::Crc16Modbus: {
        const quint16 crc = crc16Modbus(payload);
        out.append(static_cast<char>(crc & 0xFF));       // Modbus: 低字节在前
        out.append(static_cast<char>((crc >> 8) & 0xFF));
        break;
    }
    case AppendMode::Crc16Ccitt: {
        const quint16 crc = crc16CcittFalse(payload);
        out.append(static_cast<char>((crc >> 8) & 0xFF)); // CCITT: 高字节在前
        out.append(static_cast<char>(crc & 0xFF));
        break;
    }
    case AppendMode::Sum8:
        out.append(static_cast<char>(sum8(payload)));
        break;
    case AppendMode::Xor8:
        out.append(static_cast<char>(xor8(payload)));
        break;
    case AppendMode::Lrc8:
        out.append(static_cast<char>(lrc8(payload)));
        break;
    case AppendMode::None:
        break;
    }
    return out;
}

QString CrcCalculator::appendModeName(AppendMode mode)
{
    switch (mode) {
    case AppendMode::Crc16Modbus:
        return QStringLiteral("CRC16-Modbus");
    case AppendMode::Crc16Ccitt:
        return QStringLiteral("CRC16-CCITT");
    case AppendMode::Sum8:
        return QStringLiteral("Sum8");
    case AppendMode::Xor8:
        return QStringLiteral("Xor8");
    case AppendMode::Lrc8:
        return QStringLiteral("LRC8");
    case AppendMode::None:
        break;
    }
    return QStringLiteral("无");
}
