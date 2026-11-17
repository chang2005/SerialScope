#include "core/parser/HexCodec.h"

#include <QObject>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <string>
#endif

namespace {

bool isHexDigit(QChar c)
{
    return (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
        || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
        || (c >= QLatin1Char('A') && c <= QLatin1Char('F'));
}

QTextCodec *gbkCodec()
{
    static QTextCodec *codec = []() -> QTextCodec * {
        const char *names[] = {"GBK", "GB18030", "GB2312"};
        for (const char *name : names) {
            if (QTextCodec *c = QTextCodec::codecForName(name))
                return c;
        }
        return nullptr;
    }();
    return codec;
}

#ifdef Q_OS_WIN
QString gbkToUnicodeWindows(const QByteArray &data)
{
    if (data.isEmpty())
        return {};
    const int chars = MultiByteToWideChar(936, 0, data.constData(), data.size(), nullptr, 0);
    if (chars <= 0)
        return QString::fromLatin1(data);
    std::wstring buffer(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(936, 0, data.constData(), data.size(), buffer.data(), chars);
    return QString::fromWCharArray(buffer.data(), chars);
}

QByteArray unicodeToGbkWindows(const QString &text)
{
    if (text.isEmpty())
        return {};
    const int bytes = WideCharToMultiByte(936, 0,
                                          reinterpret_cast<const wchar_t *>(text.utf16()),
                                          text.size(), nullptr, 0, nullptr, nullptr);
    if (bytes <= 0)
        return text.toLatin1();
    std::string buffer(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(936, 0, reinterpret_cast<const wchar_t *>(text.utf16()),
                        text.size(), buffer.data(), bytes, nullptr, nullptr);
    return QByteArray(buffer.data(), bytes);
}
#endif

} // namespace

QString textEncodingName(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Utf8: return QStringLiteral("UTF-8");
    case TextEncoding::Gbk:  return QStringLiteral("GBK");
    case TextEncoding::Ascii:
    default:                 return QStringLiteral("ASCII");
    }
}

TextEncoding textEncodingFromName(const QString &name)
{
    if (name.compare(QLatin1String("UTF-8"), Qt::CaseInsensitive) == 0)
        return TextEncoding::Utf8;
    if (name.compare(QLatin1String("GBK"), Qt::CaseInsensitive) == 0)
        return TextEncoding::Gbk;
    return TextEncoding::Ascii;
}

QByteArray HexCodec::parseHex(const QString &text, QString *error)
{
    if (error)
        error->clear();

    QString digits;
    digits.reserve(text.size());
    const int n = text.size();

    for (int i = 0; i < n; ++i) {
        const QChar c = text.at(i);
        if (c.isSpace() || c == QLatin1Char(',') || c == QLatin1Char(';')
            || c == QLatin1Char('_')) {
            continue;
        }

        const bool prevIsHex = i > 0 && isHexDigit(text.at(i - 1));
        if (c == QLatin1Char('0') && i + 1 < n
            && (text.at(i + 1) == QLatin1Char('x') || text.at(i + 1) == QLatin1Char('X'))
            && !prevIsHex) {
            ++i; // 跳过 "0x" 前缀
            continue;
        }

        if (isHexDigit(c)) {
            digits.append(c);
            continue;
        }

        if (error) {
            *error = QObject::tr("位置 %1 出现非法字符 '%2'")
                         .arg(i + 1)
                         .arg(c);
        }
        return {};
    }

    if (digits.size() % 2 != 0) {
        if (error)
            *error = QObject::tr("HEX 数据包含奇数个十六进制字符");
        return {};
    }

    return QByteArray::fromHex(digits.toLatin1());
}

QString HexCodec::formatHex(const QByteArray &data, int bytesPerLine)
{
    if (data.isEmpty())
        return {};

    const QString hex = QString::fromLatin1(data.toHex(' ').toUpper());
    if (bytesPerLine <= 0)
        return hex;

    QStringList lines;
    lines.reserve((data.size() + bytesPerLine - 1) / bytesPerLine);
    for (int offset = 0; offset < data.size(); offset += bytesPerLine) {
        const QByteArray block = data.mid(offset, bytesPerLine);
        lines.append(QString::fromLatin1(block.toHex(' ').toUpper()));
    }
    return lines.join(QLatin1Char('\n'));
}

QTextCodec *HexCodec::codecFor(TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Utf8:
        return QTextCodec::codecForName("UTF-8");
    case TextEncoding::Gbk:
        return gbkCodec();
    case TextEncoding::Ascii:
    default:
        return nullptr;
    }
}

QString HexCodec::decode(const QByteArray &data, TextEncoding encoding,
                         QTextCodec::ConverterState *state)
{
    if (data.isEmpty())
        return {};

    switch (encoding) {
    case TextEncoding::Ascii: {
        QString out;
        out.reserve(data.size());
        for (const char raw : data) {
            const unsigned char byte = static_cast<unsigned char>(raw);
            if (byte == '\n' || byte == '\r' || byte == '\t') {
                out.append(QChar(byte));
            } else if (byte >= 0x20 && byte <= 0x7E) {
                out.append(QChar(byte));
            } else {
                out.append(QChar(0x00B7)); // 中点，表示不可打印字节
            }
        }
        return out;
    }
    case TextEncoding::Utf8:
    case TextEncoding::Gbk: {
        if (QTextCodec *codec = codecFor(encoding))
            return codec->toUnicode(data.constData(), data.size(), state);
#ifdef Q_OS_WIN
        if (encoding == TextEncoding::Gbk)
            return gbkToUnicodeWindows(data);
#endif
        return QString::fromLatin1(data);
    }
    default:
        return QString::fromLatin1(data);
    }
}

QByteArray HexCodec::encode(const QString &text, TextEncoding encoding)
{
    switch (encoding) {
    case TextEncoding::Ascii: {
        QByteArray out;
        out.reserve(text.size());
        for (const QChar c : text) {
            out.append(c.unicode() < 0x80 ? static_cast<char>(c.unicode()) : '?');
        }
        return out;
    }
    case TextEncoding::Utf8:
        return text.toUtf8();
    case TextEncoding::Gbk: {
        if (QTextCodec *codec = codecFor(TextEncoding::Gbk))
            return codec->fromUnicode(text);
#ifdef Q_OS_WIN
        return unicodeToGbkWindows(text);
#else
        return text.toUtf8();
#endif
    }
    default:
        return text.toUtf8();
    }
}
