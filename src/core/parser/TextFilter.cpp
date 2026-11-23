#include "core/parser/TextFilter.h"

#include <QStringList>

TextFilter::TextFilter() = default;

bool TextFilter::setPattern(const QString &pattern, bool useRegex)
{
    m_pattern = pattern;
    m_useRegex = useRegex;
    m_active = !pattern.isEmpty();

    m_regexValid = false;
    m_regex = QRegularExpression();
    m_effectiveRegex = false;

    if (!m_active)
        return true;

    if (!useRegex)
        return true;

    m_regex = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
    m_regexValid = m_regex.isValid();
    if (!m_regexValid) {
        // 正则非法：退回纯文本匹配，避免过滤器静默失效
        m_regex = QRegularExpression();
        return false;
    }
    m_effectiveRegex = true;
    return true;
}

QList<QPair<int, int>> TextFilter::matchRanges(const QString &text) const
{
    QList<QPair<int, int>> ranges;
    if (!m_active || text.isEmpty())
        return ranges;

    if (m_effectiveRegex) {
        QRegularExpressionMatchIterator it = m_regex.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            const int start = match.capturedStart();
            const int len = match.capturedLength();
            if (len <= 0)
                continue;
            ranges.append({start, start + len});
        }
        return ranges;
    }

    int from = 0;
    while (true) {
        const int index = text.indexOf(m_pattern, from, Qt::CaseInsensitive);
        if (index < 0)
            break;
        ranges.append({index, index + m_pattern.size()});
        from = index + qMax(1, m_pattern.size());
    }
    return ranges;
}

bool TextFilter::matchesLine(const QString &line) const
{
    if (!m_active)
        return true;
    return !matchRanges(line).isEmpty();
}

QString TextFilter::highlightMarkers(const QString &text,
                                      const QList<QPair<int, int>> &ranges)
{
    if (ranges.isEmpty())
        return text;

    QString out;
    out.reserve(text.size() + ranges.size() * 13);

    int cursor = 0;
    for (const QPair<int, int> &range : ranges) {
        if (range.first < cursor)
            continue;
        out.append(text.mid(cursor, range.first - cursor));
        out.append(QStringLiteral("<mark>"));
        out.append(text.mid(range.first, range.second - range.first));
        out.append(QStringLiteral("</mark>"));
        cursor = range.second;
    }
    out.append(text.mid(cursor));
    return out;
}
