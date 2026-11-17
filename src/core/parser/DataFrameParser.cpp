#include "core/parser/DataFrameParser.h"

#include "utils/CrcCalculator.h"

#include <QtEndian>

void DataFrameParser::setSpec(const FrameSpec &spec)
{
    m_spec = spec;
    if (m_spec.maxFrameLength < 8)
        m_spec.maxFrameLength = 8;
    if (m_spec.fixedFrameLength < 1)
        m_spec.fixedFrameLength = 1;
    reset();
}

void DataFrameParser::reset()
{
    m_buffer.clear();
}

void DataFrameParser::discard(int count)
{
    if (count <= 0)
        return;
    m_buffer.remove(0, count);
    if (onBytesDiscarded)
        onBytesDiscarded(count);
}

void DataFrameParser::feed(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    m_buffer.append(data);

    // 硬上限：正常帧不会超过 maxFrameLength，超出说明配置/数据异常
    const int bufferCap = m_spec.maxFrameLength * 2;
    if (m_buffer.size() > bufferCap)
        discard(m_buffer.size() - m_spec.maxFrameLength);

    // 无帧头的固定帧长模式：按固定步长直接切帧
    if (m_spec.header.isEmpty()) {
        if (m_spec.lengthMode != FrameSpec::LengthMode::Fixed)
            return;
        const int frameLen = m_spec.fixedFrameLength;
        while (m_buffer.size() >= frameLen) {
            const QByteArray frame = m_buffer.left(frameLen);
            if (m_spec.verifyCrc16Modbus && !verifyCrc16(frame)) {
                discard(1); // 校验失败：滑动 1 字节重新同步
                continue;
            }
            m_buffer.remove(0, frameLen);
            if (onFrame)
                onFrame(frame);
        }
        return;
    }

    const int headerSize = m_spec.header.size();

    while (true) {
        const int pos = m_buffer.indexOf(m_spec.header);
        if (pos < 0) {
            // 保留可能是半个帧头的尾部字节，其余作为噪声丢弃
            const int keep = qMin(m_buffer.size(), headerSize - 1);
            const int drop = m_buffer.size() - keep;
            if (drop > 0)
                discard(drop);
            return;
        }
        if (pos > 0)
            discard(pos);

        int frameLen = 0;
        if (m_spec.lengthMode == FrameSpec::LengthMode::Fixed) {
            frameLen = m_spec.fixedFrameLength;
        } else {
            if (m_spec.lengthByteOffset < 0
                || m_spec.lengthByteOffset >= m_spec.maxFrameLength) {
                reset();
                return;
            }
            if (m_buffer.size() <= m_spec.lengthByteOffset)
                return; // 长度字节尚未到达
            const int lengthValue = static_cast<quint8>(m_buffer.at(m_spec.lengthByteOffset));
            frameLen = lengthValue + m_spec.lengthAdjust;
        }

        if (frameLen < headerSize + 1 || frameLen > m_spec.maxFrameLength
            || (m_spec.lengthMode == FrameSpec::LengthMode::LengthByte
                && m_spec.lengthByteOffset >= frameLen)) {
            discard(1); // 非法帧长：丢弃当前帧头首字节，重新搜索
            continue;
        }

        if (m_buffer.size() < frameLen)
            return; // 半包：等待后续数据

        const QByteArray frame = m_buffer.left(frameLen);

        if (m_spec.verifyCrc16Modbus && !verifyCrc16(frame)) {
            discard(1); // CRC 失败：丢 1 字节重新同步
            continue;
        }

        m_buffer.remove(0, frameLen);
        if (onFrame)
            onFrame(frame);
    }
}

bool DataFrameParser::verifyCrc16(const QByteArray &frame) const
{
    if (frame.size() < 3)
        return false;
    const quint16 expected = qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar *>(frame.constData() + frame.size() - 2));
    const quint16 actual = CrcCalculator::crc16Modbus(
        reinterpret_cast<const quint8 *>(frame.constData()), frame.size() - 2);
    return expected == actual;
}
