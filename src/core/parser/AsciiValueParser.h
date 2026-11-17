#pragma once

#include <QByteArray>
#include <QVector>

#include <functional>

/// ASCII 行数值解析器：按 \n / \r 切行，提取每行中的全部十进制数字，
/// 按出现顺序映射为通道值。
///
/// 例："12.3, 45.6, 78.9\r\n" → {12.3, 45.6, 78.9}
/// 支持负数、小数、科学计数法；十六进制常量与字母后的数字（如 CH1）会被忽略。
class AsciiValueParser
{
public:
    using ValuesCallback = std::function<void(const QVector<double> &values)>;

    void setMaxChannels(int maxChannels);
    int maxChannels() const { return m_maxChannels; }

    void feed(const QByteArray &data);
    void reset();

    ValuesCallback onValues;
    std::function<void()> onOverflow;

private:
    void parseLine(const QByteArray &line);

    QByteArray m_buffer;
    int m_maxChannels = 8;
};
