#include "core/AutoReply.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace {

/// 判断一条规则是否命中数据。
bool ruleMatches(const AutoReplyRule &rule, const QByteArray &data)
{
    if (!rule.enabled || rule.matchPattern.isEmpty())
        return false;

    if (rule.useRegex) {
        QRegularExpression regex(rule.matchPattern,
                                 QRegularExpression::CaseInsensitiveOption);
        if (!regex.isValid())
            return false;
        // 正则按字符串语义匹配：数据先按 Latin-1 视为字节序列
        const QString text = QString::fromLatin1(data);
        return regex.match(text).hasMatch();
    }

    return data.contains(rule.matchPattern.toLatin1());
}

} // namespace

bool AutoReplyEngine::addRule(const AutoReplyRule &rule)
{
    if (!rule.isValid())
        return false;

    AutoReplyRule entry = rule;
    if (entry.name.trimmed().isEmpty())
        entry.name = entry.matchPattern.left(16);

    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules.at(i).name == entry.name) {
            m_rules[i] = entry;
            return true;
        }
    }

    m_rules.append(entry);
    while (m_rules.size() > kMaxRules)
        m_rules.removeFirst();
    return true;
}

bool AutoReplyEngine::updateRule(const QString &name, const AutoReplyRule &rule)
{
    const QString key = name.trimmed();
    if (key.isEmpty() || !rule.isValid())
        return false;

    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules.at(i).name == key) {
            AutoReplyRule entry = rule;
            entry.name = entry.name.trimmed().isEmpty() ? key : entry.name.trimmed();
            m_rules[i] = entry;
            return true;
        }
    }
    return false;
}

bool AutoReplyEngine::removeRule(const QString &name)
{
    const QString key = name.trimmed();
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules.at(i).name == key) {
            m_rules.remove(i);
            return true;
        }
    }
    return false;
}

void AutoReplyEngine::clear()
{
    m_rules.clear();
}

bool AutoReplyEngine::match(const QByteArray &data, AutoReplyRule *found,
                             QByteArray *reply) const
{
    if (data.isEmpty())
        return false;

    for (const AutoReplyRule &rule : m_rules) {
        if (!ruleMatches(rule, data))
            continue;

        if (found)
            *found = rule;

        if (reply) {
            const QByteArray expanded = expandEscapes(rule.replyPayload);
            if (rule.replyIsHex) {
                // HEX 回复：去掉空白后解析
                QString hex = rule.replyPayload;
                hex.remove(QRegularExpression(QStringLiteral("\\s")));
                if (hex.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
                    hex = hex.mid(2);
                const bool evenLength = (hex.size() % 2) == 0;
                const bool allHex = !hex.isEmpty()
                    && std::all_of(hex.cbegin(), hex.cend(), [](QChar c) {
                           return c.isDigit() || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
                               || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
                       });
                if (evenLength && allHex) {
                    *reply = QByteArray::fromHex(hex.toLatin1());
                } else {
                    // 不是合法 HEX，退回按文本处理
                    *reply = expanded;
                }
            } else {
                *reply = expanded;
            }
        }
        return true;
    }
    return false;
}

QByteArray AutoReplyEngine::expandEscapes(const QString &text)
{
    QByteArray out;
    out.reserve(text.size());

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch != QLatin1Char('\\') || i + 1 >= text.size()) {
            out.append(QString(ch).toUtf8());
            continue;
        }

        const QChar next = text.at(++i);
        switch (next.unicode()) {
        case 'r':
            out.append('\r');
            break;
        case 'n':
            out.append('\n');
            break;
        case 't':
            out.append('\t');
            break;
        case '0':
            out.append('\0');
            break;
        case '\\':
            out.append('\\');
            break;
        case 'x': {
            if (i + 2 < text.size()) {
                bool ok = false;
                const int value = text.mid(i + 1, 2).toInt(&ok, 16);
                if (ok) {
                    out.append(static_cast<char>(value));
                    i += 2;
                    break;
                }
            }
            out.append('\\');
            out.append(QString(next).toUtf8());
            break;
        }
        default:
            out.append('\\');
            out.append(QString(next).toUtf8());
            break;
        }
    }
    return out;
}

QString AutoReplyEngine::toJson() const
{
    QJsonArray array;
    for (const AutoReplyRule &rule : m_rules) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), rule.name);
        obj.insert(QStringLiteral("match"), rule.matchPattern);
        obj.insert(QStringLiteral("reply"), rule.replyPayload);
        obj.insert(QStringLiteral("regex"), rule.useRegex);
        obj.insert(QStringLiteral("replyHex"), rule.replyIsHex);
        obj.insert(QStringLiteral("enabled"), rule.enabled);
        array.append(obj);
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool AutoReplyEngine::fromJson(const QString &json)
{
    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray())
        return false;

    QVector<AutoReplyRule> parsed;
    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        AutoReplyRule rule;
        rule.name = obj.value(QStringLiteral("name")).toString();
        rule.matchPattern = obj.value(QStringLiteral("match")).toString();
        rule.replyPayload = obj.value(QStringLiteral("reply")).toString();
        rule.useRegex = obj.value(QStringLiteral("regex")).toBool(false);
        rule.replyIsHex = obj.value(QStringLiteral("replyHex")).toBool(false);
        rule.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
        if (!rule.isValid())
            continue;
        if (rule.name.trimmed().isEmpty())
            rule.name = rule.matchPattern.left(16);
        parsed.append(rule);
    }

    while (parsed.size() > kMaxRules)
        parsed.removeFirst();

    m_rules = parsed;
    return true;
}
