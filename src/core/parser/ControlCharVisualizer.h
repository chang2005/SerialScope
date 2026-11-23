#pragma once

#include <QString>

/// 控制字符可视化：把 \r \n \t 等不可见字符显示为可读符号。
///
/// 用于诊断换行符不匹配这类常见通信问题——肉眼看不到 \r 与 \n 的区别，
/// 但它们对设备端协议往往完全不同。
class ControlCharVisualizer
{
public:
    /// 构造器。showAll 为 true 时把全部 C0 控制字符都转义显示，
    /// 为 false 时只转义换行、回车、制表这三个最常见的。
    explicit ControlCharVisualizer(bool showAll = false);

    /// 转义单个字符：不可见字符返回形如 "<CR>" 的记号，
    /// 可打印字符原样返回。
    QString escape(QChar ch) const;

    /// 转义整段文本。
    QString escape(const QString &text) const;

    bool showsAll() const { return m_showAll; }
    void setShowsAll(bool showAll) { m_showAll = showAll; }

private:
    bool m_showAll = false;
};
