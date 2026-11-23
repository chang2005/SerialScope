#pragma once

#include <QByteArray>

#include <functional>

/// 帧尾与校验的组合方式
///
/// 帧尾固定占 tail.size() 字节，位于校验字段之后。
enum class FrameTailMode {
    None,       ///< 无帧尾
    Fixed       ///< 固定字节序列，必须逐字节匹配
};

/// 帧的校验算法
enum class FrameChecksum {
    None,          ///< 不校验
    Crc16Modbus,   ///< CRC16-Modbus，小端，2 字节
    Crc16Ccitt,    ///< CRC16-CCITT-FALSE，高字节在前，2 字节
    Sum8,          ///< 8 位累加和，1 字节
    Xor8,          ///< 8 位异或，1 字节
    Lrc8,          ///< LRC（补码），1 字节
};

/// 帧格式定义
struct FrameSpec {
    QByteArray header = QByteArray::fromHex("AA55"); ///< 帧头（可为空：固定帧长模式）
    enum class LengthMode { Fixed, LengthByte };
    LengthMode lengthMode = LengthMode::LengthByte;
    int lengthByteOffset = 2;   ///< 长度字节在帧内的偏移
    int lengthAdjust = 6;       ///< 帧长 = 长度字节值 + lengthAdjust
    int fixedFrameLength = 16;  ///< 固定帧长模式下的帧长度

    FrameChecksum checksum = FrameChecksum::None; ///< 帧校验算法（None 表示不校验）
    QByteArray tail;             ///< 帧尾字节序列（为空表示无帧尾）
    FrameTailMode tailMode = FrameTailMode::None;

    int maxFrameLength = 4096;  ///< 合法帧长上限，超过则视为噪声

    /// 校验字段占用的字节数（0 表示无校验字段）
    int checksumLength() const;
    /// 帧尾占用的字节数
    int tailLength() const;
    /// 帧尾之后紧跟校验、校验之后紧跟帧尾，取决于校验算法位置约定
    ///
    /// 本实现约定：帧内顺序为 [帧头][长度/数据][校验][帧尾]
    bool hasChecksum() const { return checksum != FrameChecksum::None; }
    bool hasTail() const { return tailMode != FrameTailMode::None && !tail.isEmpty(); }
};

/// 增量帧解析状态机：支持帧头搜索、噪声跳过、半包等待、粘包拆分、
/// 非法长度重同步、可选校验与可选帧尾。
class DataFrameParser
{
public:
    void setSpec(const FrameSpec &spec);
    const FrameSpec &spec() const { return m_spec; }

    void feed(const QByteArray &data);
    void reset();

    std::function<void(const QByteArray &frame)> onFrame;
    std::function<void(int bytesDiscarded)> onBytesDiscarded;

private:
    /// 按 spec.checksum 校验帧末的校验字段
    bool verifyChecksum(const QByteArray &frame) const;
    /// 校验帧尾是否匹配
    bool verifyTail(const QByteArray &frame) const;
    void discard(int count);

    FrameSpec m_spec;
    QByteArray m_buffer;
};
