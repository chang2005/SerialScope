#include "core/QuickSend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool QuickSendList::add(const QuickSendItem &item)
{
    if (!item.isValid())
        return false;

    QuickSendItem entry = item;
    if (entry.name.trimmed().isEmpty()) {
        // 名称缺省时取负载前 12 个字符
        QString fallback = entry.payload.trimmed();
        entry.name = fallback.left(12);
    }
    entry.name = entry.name.trimmed();

    // 同名覆盖
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).name == entry.name) {
            m_items[i] = entry;
            return true;
        }
    }

    m_items.append(entry);
    while (m_items.size() > kMaxItems)
        m_items.removeFirst();
    return true;
}

bool QuickSendList::update(const QString &name, const QuickSendItem &item)
{
    const QString key = name.trimmed();
    if (key.isEmpty() || !item.isValid())
        return false;

    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).name == key) {
            QuickSendItem entry = item;
            entry.name = entry.name.trimmed().isEmpty() ? key : entry.name.trimmed();
            m_items[i] = entry;
            return true;
        }
    }
    return false;
}

bool QuickSendList::remove(const QString &name)
{
    const QString key = name.trimmed();
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).name == key) {
            m_items.remove(i);
            return true;
        }
    }
    return false;
}

const QuickSendItem *QuickSendList::find(const QString &name) const
{
    const QString key = name.trimmed();
    for (const QuickSendItem &item : m_items) {
        if (item.name == key)
            return &item;
    }
    return nullptr;
}

void QuickSendList::clear()
{
    m_items.clear();
}

QString QuickSendList::toJson() const
{
    QJsonArray array;
    for (const QuickSendItem &item : m_items) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), item.name);
        obj.insert(QStringLiteral("payload"), item.payload);
        obj.insert(QStringLiteral("hex"), item.hex);
        array.append(obj);
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool QuickSendList::fromJson(const QString &json)
{
    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return false;

    QVector<QuickSendItem> parsed;
    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        QuickSendItem item;
        item.name = obj.value(QStringLiteral("name")).toString();
        item.payload = obj.value(QStringLiteral("payload")).toString();
        item.hex = obj.value(QStringLiteral("hex")).toBool(false);
        if (!item.isValid())
            continue;
        if (item.name.trimmed().isEmpty())
            item.name = item.payload.trimmed().left(12);
        parsed.append(item);
    }

    while (parsed.size() > kMaxItems)
        parsed.removeFirst();

    m_items = parsed;
    return true;
}
