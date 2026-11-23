#pragma once

#include <QRegularExpression>
#include <QString>

/// 接收区数据过滤器：按关键词（纯文本或正则）筛选并高亮命中内容。
///
/// 过滤器本身不依赖 UI：匹配逻辑集中在 matchRanges()，便于单元测试；
/// 高亮渲染由 UI 层根据返回的区间自行着色。
class TextFilter
{
public:
    TextFilter();

    /// 设置过滤关键词。空字符串表示不过滤（全部命中）。
    /// useRegex 为 true 时按正则解析，非法正则会退回纯文本匹配并返回 false。
    bool setPattern(const QString &pattern, bool useRegex);

    QString pattern() const { return m_pattern; }
    bool useRegex() const { return m_useRegex; }
    bool isActive() const { return m_active; }

    /// 文本中命中的区间列表（成对：start, end）。
    /// 纯文本匹配为大小写不敏感；正则按 QRegularExpression 的默认语义。
    QList<QPair<int, int>> matchRanges(const QString &text) const;

    /// 判断一行是否应当显示。过滤器未启用时恒为 true。
    bool matchesLine(const QString &line) const;

    /// 把命中片段替换为高亮标记，便于在不支持富文本的场合复用。
    /// 例如 highlightMarkers(text) → "<mark>abc</mark>"。
    static QString highlightMarkers(const QString &text,
                                    const QList<QPair<int, int>> &ranges);

private:
    QString m_pattern;
    bool m_useRegex = false;   ///< 用户请求的模式（正则 / 纯文本）
    bool m_effectiveRegex = false; ///< 实际按正则匹配（正则非法时为 false）
    bool m_active = false;
    bool m_regexValid = false;
    QRegularExpression m_regex;
};
