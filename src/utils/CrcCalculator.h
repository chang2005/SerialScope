#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

/// 常用 CRC / 校验和算法
class CrcCalculator
{
public:
    /// CRC-8/ATM：poly 0x07，init 0x00
    static quint8 crc8(const quint8 *data, int length);
    static quint8 crc8(const QByteArray &data);

    /// CRC-16/MODBUS：poly 0xA001（反射），init 0xFFFF
    static quint16 crc16Modbus(const quint8 *data, int length);
    static quint16 crc16Modbus(const QByteArray &data);

    /// CRC-16/CCITT-FALSE：poly 0x1021，init 0xFFFF
    static quint16 crc16CcittFalse(const quint8 *data, int length);
    static quint16 crc16CcittFalse(const QByteArray &data);

    /// CRC-32/IEEE（zip 常用）
    static quint32 crc32(const quint8 *data, int length);
    static quint32 crc32(const QByteArray &data);

    /// 8 位累加和（模 256）
    static quint8 sum8(const QByteArray &data);

    /// 8 位异或校验
    static quint8 xor8(const QByteArray &data);

    /// LRC（纵向冗余校验，Modbus ASCII 模式使用）：所有字节求和后取补码
    static quint8 lrc8(const QByteArray &data);

    /// 将校验值追加到数据末尾的模式
    enum class AppendMode { None, Crc16Modbus, Crc16Ccitt, Sum8, Xor8, Lrc8 };

    static QByteArray appendChecksum(const QByteArray &payload, AppendMode mode);

    /// 追加模式的显示名（用于 UI 下拉框）
    static QString appendModeName(AppendMode mode);
};
