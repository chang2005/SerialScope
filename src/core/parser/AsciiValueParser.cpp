#include "core/parser/AsciiValueParser.h"

#include <QRegularExpression>
#include <QString>

#include <cmath>

namespace {
constexpr int kMaxBufferedBytes = 64 * 1024; // 半行缓冲上限

const QRegularExpression &numberRegex()
{
    static const QRegularExpression re(
        QStringLiteral(R"([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)"));
    return re;
}
} // namespace

void AsciiValueParser::setMaxChannels(int maxChannels)
{
    m_maxChannels = qMax(1, maxChannels);
}

void AsciiValueParser::reset()
{
    m_buffer.clear();
}

void AsciiValueParser::feed(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    m_buffer.append(data);

    if (m_buffer.size() > kMaxBufferedBytes) {
        // 长时间收不到换行（可能是二进制噪声），丢弃以免无限增长
        m_buffer.clear();
        if (onOverflow)
            onOverflow();
        return;
    }

    while (true) {
        int delimiter = -1;
        for (int i = 0; i < m_buffer.size(); ++i) {
            const char c = m_buffer.at(i);
            if (c == '\n' || c == '\r') {
                delimiter = i;
                break;
            }
        }
        if (delimiter < 0)
            break;

        const QByteArray line = m_buffer.left(delimiter);
        const char delim = m_buffer.at(delimiter);
        m_buffer.remove(0, delimiter + 1);

        // 吃掉 CRLF / LFCR 的第二个字符
        if (!m_buffer.isEmpty()) {
            const char next = m_buffer.at(0);
            if ((delim == '\r' && next == '\n') || (delim == '\n' && next == '\r'))
                m_buffer.remove(0, 1);
        }

        if (!line.trimmed().isEmpty())
            parseLine(line);
    }
}

void AsciiValueParser::parseLine(const QByteArray &line)
{
    const QString text = QString::fromLatin1(line);
    QVector<double> values;

    auto it = numberRegex().globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const int start = match.capturedStart();
        const QString token = match.captured();

        // 跳过字母/下划线后的数字（CH1、0x1F 等）以及残缺的小数点连接
        if (start > 0) {
            const QChar prev = text.at(start - 1);
            if (prev.isLetter() || prev == QLatin1Char('_') || prev == QLatin1Char('.'))
                continue;
        }
        if (token == QLatin1String("0")
            && match.capturedEnd() < text.size()
            && (text.at(match.capturedEnd()) == QLatin1Char('x')
                || text.at(match.capturedEnd()) == QLatin1Char('X'))) {
            continue;
        }

        bool ok = false;
        const double value = token.toDouble(&ok);
        if (ok && std::isfinite(value))
            values.append(value);

        if (values.size() >= m_maxChannels)
            break;
    }

    if (!values.isEmpty() && onValues)
        onValues(values);
}
