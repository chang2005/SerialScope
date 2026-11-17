#pragma once

#include <QByteArray>

#include <functional>

/// 帧格式定义
struct FrameSpec {
    QByteArray header = QByteArray::fromHex("AA55"); ///< 帧头（可为空：固定帧长模式）
    enum class LengthMode { Fixed, LengthByte };
    LengthMode lengthMode = LengthMode::LengthByte;
    int lengthByteOffset = 2;   ///< 长度字节在帧内的偏移
    int lengthAdjust = 6;       ///< 帧长 = 长度字节值 + lengthAdjust
    int fixedFrameLength = 16;  ///< 固定帧长模式下的帧长度
    bool verifyCrc16Modbus = false; ///< 校验帧末 2 字节（CRC16-Modbus，小端）
    int maxFrameLength = 4096;  ///< 合法帧长上限，超过则视为噪声
};

/// 增量帧解析状态机：支持帧头搜索、噪声跳过、半包等待、粘包拆分、
/// 非法长度重同步与可选 CRC16 校验。
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
    bool verifyCrc16(const QByteArray &frame) const;
    void discard(int count);

    FrameSpec m_spec;
    QByteArray m_buffer;
};
