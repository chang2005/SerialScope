#include "core/parser/ControlCharVisualizer.h"

ControlCharVisualizer::ControlCharVisualizer(bool showAll)
    : m_showAll(showAll)
{
}

QString ControlCharVisualizer::escape(QChar ch) const
{
    const char16_t code = ch.unicode();

    switch (code) {
    case '\r':
        return QStringLiteral("<CR>");
    case '\n':
        return QStringLiteral("<LF>");
    case '\t':
        return QStringLiteral("<TAB>");
    case '\0':
        return QStringLiteral("<NUL>");
    case 0x07:
        return QStringLiteral("<BEL>");
    case 0x08:
        return QStringLiteral("<BS>");
    case 0x0B:
        return QStringLiteral("<VT>");
    case 0x0C:
        return QStringLiteral("<FF>");
    case 0x1B:
        return QStringLiteral("<ESC>");
    default:
        break;
    }

    if (m_showAll && code < 0x20)
        return QStringLiteral("<0x%1>")
            .arg(QString::number(code, 16).rightJustified(2, QLatin1Char('0')).toUpper());
    if (m_showAll && code == 0x7F)
        return QStringLiteral("<DEL>");

    return ch;
}

QString ControlCharVisualizer::escape(const QString &text) const
{
    if (text.isEmpty())
        return text;

    bool needsEscape = false;
    for (const QChar ch : text) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7F) {
            needsEscape = true;
            break;
        }
    }
    if (!needsEscape)
        return text;

    QString out;
    out.reserve(text.size() + 16);
    for (const QChar ch : text)
        out.append(escape(ch));
    return out;
}
