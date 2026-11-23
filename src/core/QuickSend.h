#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

/// 快捷发送条目：名称 + 负载 + 是否为 HEX 模式。
struct QuickSendItem {
    QString name;
    QString payload;   ///< 文本模式为原始字符串；HEX 模式为 HEX 字符串
    bool hex = false;

    bool isValid() const { return !payload.trimmed().isEmpty(); }
};

/// 快捷发送清单：有序的条目集合，支持增删改查与持久化序列化。
///
/// 保持无 UI 依赖，便于单元测试；界面层只负责编辑与呈现。
class QuickSendList
{
public:
    /// 上限条目数，超出时最旧的条目会被丢弃。
    static constexpr int kMaxItems = 64;

    const QVector<QuickSendItem> &items() const { return m_items; }
    int count() const { return m_items.size(); }
    bool isEmpty() const { return m_items.isEmpty(); }

    /// 追加条目。名称为空时用负载前若干字符代替；超过上限时丢弃最旧的。
    /// 返回是否真的加入（同名条目会覆盖旧条目）。
    bool add(const QuickSendItem &item);

    /// 按名称更新条目；名称不存在返回 false。
    bool update(const QString &name, const QuickSendItem &item);

    /// 按名称删除；不存在返回 false。
    bool remove(const QString &name);

    /// 按名称查找，找不到返回 nullptr。
    const QuickSendItem *find(const QString &name) const;

    /// 清空全部条目。
    void clear();

    /// 序列化为可保存的 JSON 数组文本。
    QString toJson() const;

    /// 从 JSON 文本恢复；格式非法时返回 false 且不改变当前内容。
    bool fromJson(const QString &json);

private:
    QVector<QuickSendItem> m_items;
};
