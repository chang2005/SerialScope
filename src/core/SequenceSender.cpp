#include "core/SequenceSender.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>

int SequenceSet::addGroup(const SequenceGroup &group)
{
    if (group.items.isEmpty())
        return -1;

    SequenceGroup entry = group;
    entry.name = entry.name.trimmed();

    // 组名重复时自动加序号，避免界面上出现两个同名组
    if (entry.name.isEmpty())
        entry.name = QObject::tr("组 %1").arg(m_groups.size() + 1);
    QString base = entry.name;
    int suffix = 2;
    bool duplicated = true;
    while (duplicated) {
        duplicated = false;
        for (const SequenceGroup &existing : m_groups) {
            if (existing.name == entry.name) {
                entry.name = QStringLiteral("%1 (%2)").arg(base).arg(suffix++);
                duplicated = true;
                break;
            }
        }
    }

    if (entry.intervalMs < 10)
        entry.intervalMs = 10;

    // 单组条目数上限
    while (entry.items.size() > kMaxItemsPerGroup)
        entry.items.removeLast();

    m_groups.append(entry);
    while (m_groups.size() > kMaxGroups)
        m_groups.removeFirst();
    return m_groups.size() - 1;
}

bool SequenceSet::updateGroup(int index, const SequenceGroup &group)
{
    if (index < 0 || index >= m_groups.size())
        return false;

    SequenceGroup entry = group;
    entry.name = entry.name.trimmed();
    if (entry.name.isEmpty())
        entry.name = QObject::tr("组 %1").arg(index + 1);
    if (entry.intervalMs < 10)
        entry.intervalMs = 10;
    while (entry.items.size() > kMaxItemsPerGroup)
        entry.items.removeLast();

    m_groups[index] = entry;
    return true;
}

bool SequenceSet::removeGroup(int index)
{
    if (index < 0 || index >= m_groups.size())
        return false;
    m_groups.remove(index);
    return true;
}

const SequenceGroup *SequenceSet::groupAt(int index) const
{
    if (index < 0 || index >= m_groups.size())
        return nullptr;
    return &m_groups.at(index);
}

void SequenceSet::clear()
{
    m_groups.clear();
}

QString SequenceSet::toJson() const
{
    QJsonArray array;
    for (const SequenceGroup &group : m_groups) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), group.name);
        obj.insert(QStringLiteral("intervalMs"), group.intervalMs);
        QJsonArray items;
        for (const SequenceItem &item : group.items) {
            QJsonObject itemObj;
            itemObj.insert(QStringLiteral("label"), item.label);
            itemObj.insert(QStringLiteral("payload"), QString::fromLatin1(item.payload.toHex()));
            items.append(itemObj);
        }
        obj.insert(QStringLiteral("items"), items);
        array.append(obj);
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool SequenceSet::fromJson(const QString &json)
{
    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return false;

    QVector<SequenceGroup> parsed;
    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        SequenceGroup group;
        group.name = obj.value(QStringLiteral("name")).toString();
        group.intervalMs = obj.value(QStringLiteral("intervalMs")).toInt(1000);
        if (group.intervalMs < 10)
            group.intervalMs = 10;

        const QJsonArray items = obj.value(QStringLiteral("items")).toArray();
        for (const QJsonValue &itemValue : items) {
            if (!itemValue.isObject())
                continue;
            const QJsonObject itemObj = itemValue.toObject();
            SequenceItem item;
            item.label = itemObj.value(QStringLiteral("label")).toString();
            const QByteArray hex =
                QByteArray::fromHex(itemObj.value(QStringLiteral("payload")).toString().toLatin1());
            item.payload = hex;
            if (item.isValid())
                group.items.append(item);
        }
        if (group.items.isEmpty())
            continue;
        parsed.append(group);
    }

    while (parsed.size() > kMaxGroups)
        parsed.removeFirst();

    m_groups = parsed;
    return true;
}

void SequenceRunner::setGroups(const QVector<SequenceGroup> &groups)
{
    m_groups = groups;
    m_groupIndex = 0;
    m_itemIndex = 0;
}

bool SequenceRunner::start()
{
    m_groupIndex = 0;
    m_itemIndex = 0;
    // 跳过开头的空组
    int guard = 0;
    while (m_groupIndex < m_groups.size() && guard++ < 1000) {
        if (!m_groups.at(m_groupIndex).items.isEmpty())
            break;
        ++m_groupIndex;
        m_itemIndex = 0;
    }
    if (m_groupIndex >= m_groups.size()) {
        m_running = false;
        return false;
    }
    m_running = true;
    return true;
}

void SequenceRunner::stop()
{
    m_running = false;
}

bool SequenceRunner::currentPayload(QByteArray *payload, QString *label) const
{
    if (m_groupIndex < 0 || m_groupIndex >= m_groups.size())
        return false;
    const SequenceGroup &group = m_groups.at(m_groupIndex);
    if (m_itemIndex < 0 || m_itemIndex >= group.items.size())
        return false;
    const SequenceItem &item = group.items.at(m_itemIndex);
    if (item.payload.isEmpty())
        return false;
    if (payload)
        *payload = item.payload;
    if (label)
        *label = item.label.isEmpty() ? group.name : item.label;
    return true;
}

void SequenceRunner::advance()
{
    if (m_groups.isEmpty()) {
        m_running = false;
        return;
    }

    ++m_itemIndex;
    // 当前组发完 → 下一组
    while (m_groupIndex < m_groups.size()) {
        const SequenceGroup &group = m_groups.at(m_groupIndex);
        if (m_itemIndex < group.items.size())
            return;
        ++m_groupIndex;
        m_itemIndex = 0;
    }

    // 所有组发完 → 回到第一组（循环轮发）
    m_groupIndex = 0;
    m_itemIndex = 0;
    int guard = 0;
    while (m_groupIndex < m_groups.size() && guard++ < 1000) {
        if (!m_groups.at(m_groupIndex).items.isEmpty())
            return;
        ++m_groupIndex;
        m_itemIndex = 0;
    }
    // 所有组都是空的：停机
    m_running = false;
}

QString SequenceRunner::currentLabel() const
{
    if (m_groupIndex < 0 || m_groupIndex >= m_groups.size())
        return QString();
    const SequenceGroup &group = m_groups.at(m_groupIndex);
    if (m_itemIndex < 0 || m_itemIndex >= group.items.size())
        return group.name;
    const SequenceItem &item = group.items.at(m_itemIndex);
    return item.label.isEmpty() ? group.name : item.label;
}

QString SequenceRunner::currentGroupName() const
{
    if (m_groupIndex < 0 || m_groupIndex >= m_groups.size())
        return QString();
    return m_groups.at(m_groupIndex).name;
}

int SequenceRunner::currentInterval() const
{
    if (m_groupIndex < 0 || m_groupIndex >= m_groups.size())
        return 1000;
    return m_groups.at(m_groupIndex).intervalMs;
}
