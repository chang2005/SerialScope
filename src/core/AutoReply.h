#pragma once

#include <QByteArray>
#include <QRegularExpression>
#include <QString>
#include <QVector>

/// 自动回复规则：收到匹配的请求后自动回复一条预设响应。
///
/// 典型用途是模拟从机设备，让主机程序在没有真实硬件时也能联调。
struct AutoReplyRule {
    QString name;
    QString matchPattern;   ///< 匹配内容（纯文本或正则）
    QString replyPayload;   ///< 回复内容
    bool useRegex = false;
    bool replyIsHex = false;
    bool enabled = true;

    bool isValid() const { return !matchPattern.isEmpty() && !replyPayload.isEmpty(); }
};

/// 自动回复规则集合与匹配引擎。
///
/// 匹配顺序为加入顺序，第一条命中的规则生效；回复内容里的转义占位符
/// （\r \n \t \\ \xNN）会被解析成实际字节。
class AutoReplyEngine
{
public:
    /// 单条回复规则的上限数量
    static constexpr int kMaxRules = 32;

    const QVector<AutoReplyRule> &rules() const { return m_rules; }
    int count() const { return m_rules.size(); }
    bool isEmpty() const { return m_rules.isEmpty(); }

    /// 追加规则；同名覆盖；超过上限丢弃最旧的。返回是否加入成功。
    bool addRule(const AutoReplyRule &rule);

    /// 按名称更新；不存在返回 false。
    bool updateRule(const QString &name, const AutoReplyRule &rule);

    /// 按名称删除；不存在返回 false。
    bool removeRule(const QString &name);

    void clear();

    /// 在接收数据中查找命中的规则。
    /// found 为 true 时返回该规则，reply 为解析后的回复字节。
    bool match(const QByteArray &data, AutoReplyRule *found, QByteArray *reply) const;

    /// 解析回复内容中的转义占位符：\r \n \t \\ \xNN
    static QByteArray expandEscapes(const QString &text);

    /// 序列化为 JSON 文本。
    QString toJson() const;

    /// 从 JSON 文本恢复；格式非法返回 false 且不改变当前内容。
    bool fromJson(const QString &json);

private:
    QVector<AutoReplyRule> m_rules;
};
