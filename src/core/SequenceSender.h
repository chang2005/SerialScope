#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

/// 轮发条目：一条待发送的数据
struct SequenceItem {
    QString label;      ///< 条目名称（可为空，显示时回退到组名）
    QByteArray payload; ///< 待发送的字节（已按目标编码转换完毕）

    bool isValid() const { return !payload.isEmpty(); }
};

/// 一组轮发数据
///
/// 组内条目依次发送；某一组发完即跳到下一组，全部组发完则回到第一组，
/// 如此循环，实现「多组数据自动轮流发送」。
struct SequenceGroup {
    QString name;
    QVector<SequenceItem> items;
    int intervalMs = 1000; ///< 定时轮发时该组之间的间隔（毫秒）

    bool isEmpty() const { return items.isEmpty(); }
};

/// 轮发组集合：负责增删改查与持久化序列化
///
/// 保持无 UI 依赖，便于单元测试；界面层只负责编辑与呈现。
class SequenceSet
{
public:
    static constexpr int kMaxGroups = 32;
    static constexpr int kMaxItemsPerGroup = 64;

    const QVector<SequenceGroup> &groups() const { return m_groups; }
    int count() const { return m_groups.size(); }
    bool isEmpty() const { return m_groups.isEmpty(); }

    /// 追加一组。返回新组的下标；组名重复时自动改名。
    int addGroup(const SequenceGroup &group);
    /// 按下标更新一组；下标越界返回 false。
    bool updateGroup(int index, const SequenceGroup &group);
    /// 按下标删除一组；下标越界返回 false。
    bool removeGroup(int index);
    /// 按下标取一组；下标越界返回 nullptr。
    const SequenceGroup *groupAt(int index) const;

    /// 清空全部组。
    void clear();

    /// 序列化为可保存的 JSON 文本。
    QString toJson() const;
    /// 从 JSON 文本恢复；格式非法时返回 false 且不改变当前内容。
    bool fromJson(const QString &json);

private:
    QVector<SequenceGroup> m_groups;
};

/// 轮发引擎：驱动多组数据的轮流发送
///
/// 两种触发方式：
/// - Interval：定时器按间隔触发，逐条发送（对应「定时循环发送」）
/// - Reply：发出第一条后等待应答，收到数据才发送下一条（对应「收到应答后自动发送下一条」）
///
/// 指针语义：m_groupIndex / m_itemIndex 始终指向「下一条要发送的条目」。
/// 因此发送完一条之后调用 advance() 即可。
class SequenceRunner
{
public:
    enum class Mode {
        Interval, ///< 定时轮发
        Reply,    ///< 应答触发
    };

    void setGroups(const QVector<SequenceGroup> &groups);
    const QVector<SequenceGroup> &groups() const { return m_groups; }

    void setMode(Mode mode) { m_mode = mode; }
    Mode mode() const { return m_mode; }

    /// 开始轮发。指针复位到第一条；没有可用条目时返回 false。
    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    /// 取当前指针所指的条目。指针无效或条目为空时返回 false。
    bool currentPayload(QByteArray *payload, QString *label) const;

    /// 发送完一条后调用，推进到下一条（自动跨组、跳过空组）。
    void advance();

    /// 当前进度（供界面显示）
    int currentGroupIndex() const { return m_groupIndex; }
    int currentItemIndex() const { return m_itemIndex; }
    /// 当前条目的显示名：优先条目名，其次组名
    QString currentLabel() const;
    /// 当前组名
    QString currentGroupName() const;
    /// 当前组的间隔
    int currentInterval() const;

private:
    QVector<SequenceGroup> m_groups;
    Mode m_mode = Mode::Interval;
    bool m_running = false;
    int m_groupIndex = 0;
    int m_itemIndex = 0;
};
