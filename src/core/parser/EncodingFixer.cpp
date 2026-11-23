#include "core/parser/EncodingFixer.h"

#include "core/parser/HexCodec.h"

#include <QTextCodec>

#include <algorithm>

namespace {

} // namespace

QString EncodingFixer::encodingName(Encoding encoding)
{
    switch (encoding) {
    case Encoding::Latin1:
        return QStringLiteral("Latin-1 (ISO-8859-1)");
    case Encoding::Utf8:
        return QStringLiteral("UTF-8");
    case Encoding::Gbk:
        return QStringLiteral("GBK (CP936)");
    case Encoding::Ascii:
        return QStringLiteral("ASCII");
    }
    return QStringLiteral("UTF-8");
}

QByteArray EncodingFixer::encodeText(const QString &text, Encoding encoding)
{
    switch (encoding) {
    case Encoding::Latin1:
        // ISO-8859-1：每个 Unicode 码点低 8 位对应一个字节，可无损往返
        return text.toLatin1();
    case Encoding::Utf8:
        return text.toUtf8();
    case Encoding::Gbk:
        return HexCodec::encode(text, TextEncoding::Gbk);
    case Encoding::Ascii:
        return HexCodec::encode(text, TextEncoding::Ascii);
    }
    return text.toUtf8();
}

QString EncodingFixer::decodeBytes(const QByteArray &bytes, Encoding encoding)
{
    switch (encoding) {
    case Encoding::Latin1:
        return QString::fromLatin1(bytes);
    case Encoding::Utf8:
        if (QTextCodec *codec = QTextCodec::codecForName("UTF-8"))
            return codec->toUnicode(bytes.constData(), bytes.size());
        return QString::fromUtf8(bytes);
    case Encoding::Gbk:
        return HexCodec::decode(bytes, TextEncoding::Gbk);
    case Encoding::Ascii:
        return HexCodec::decode(bytes, TextEncoding::Ascii);
    }
    return QString::fromUtf8(bytes);
}

QString EncodingFixer::fix(const QString &garbled, Encoding src, Encoding dst)
{
    if (garbled.isEmpty())
        return garbled;
    const QByteArray bytes = encodeText(garbled, src);
    if (bytes.isEmpty())
        return QString();
    return decodeBytes(bytes, dst);
}

EncodingFixer::Candidate EncodingFixer::decodeWith(const QByteArray &bytes, Encoding encoding)
{
    Candidate candidate;
    candidate.encoding = encoding;
    candidate.text = decodeBytes(bytes, encoding);
    candidate.replacementChars = 0;
    candidate.score = readabilityScore(candidate.text, &candidate.replacementChars);
    candidate.bytesAreValidUtf8 = isValidUtf8(bytes);
    return candidate;
}

bool EncodingFixer::isValidUtf8(const QByteArray &bytes)
{
    // 严格校验 UTF-8：字节序列必须完全符合多字节编码规则。
    // GBK 的中文字节对（如 B4 AE）不是合法 UTF-8，可以据此把两者区分开。
    const uchar *p = reinterpret_cast<const uchar *>(bytes.constData());
    const int size = bytes.size();
    int i = 0;
    while (i < size) {
        const uchar b = p[i];
        int extra = 0;
        if (b < 0x80) {
            ++i;
            continue;
        } else if ((b & 0xE0) == 0xC0) {
            extra = 1;
            if (b < 0xC2) // 0xC0/0xC1 属过长编码
                return false;
        } else if ((b & 0xF0) == 0xE0) {
            extra = 2;
        } else if ((b & 0xF8) == 0xF0) {
            extra = 3;
            if (b > 0xF4) // 超过 U+10FFFF
                return false;
        } else {
            return false; // 0x80-0xBF 续字节开头或 0xF5-0xFF 非法
        }

        if (i + extra >= size)
            return false; // 截断
        for (int k = 1; k <= extra; ++k) {
            if ((p[i + k] & 0xC0) != 0x80)
                return false; // 续字节格式错误
        }
        // 过长编码检查：UTF-8 最短形式
        if (extra == 2 && b == 0xE0 && p[i + 1] < 0xA0)
            return false;
        if (extra == 3 && b == 0xF0 && p[i + 1] < 0x90)
            return false;
        i += extra + 1;
    }
    return true;
}

QVector<EncodingFixer::Candidate> EncodingFixer::candidates(const QByteArray &bytes)
{
    const Encoding all[] = {Encoding::Utf8, Encoding::Gbk, Encoding::Latin1, Encoding::Ascii};

    QVector<Candidate> result;
    result.reserve(4);
    for (const Encoding encoding : all)
        result.append(decodeWith(bytes, encoding));

    // 关键判据：UTF-8 是自描述编码，字节序列要么完全合法、要么解码时立刻报错。
    // GBK 的中文字节对（B4 AE 之类）不是合法 UTF-8，因此：
    //   - 字节是合法 UTF-8  -> 几乎可以断定原文就是 UTF-8
    //   - 字节不是合法 UTF-8 -> 几乎可以断定原文是 GBK 之类的双字节编码
    // 只看解码后的字符可读性是不够的：Latin-1 / ASCII 对任意字节都"解得出"，
    // 但结果全是无意义的替换字符，评分反而会虚高。
    const bool validUtf8 = result.at(0).bytesAreValidUtf8;
    for (Candidate &candidate : result) {
        if (candidate.encoding == Encoding::Utf8) {
            candidate.score += validUtf8 ? 40 : -40;
        } else if (candidate.encoding == Encoding::Gbk) {
            candidate.score += validUtf8 ? -40 : 40;
        } else {
            // Latin-1 / ASCII 对任意字节都能解出「合法字符」，但内容多半是垃圾
            candidate.score -= 10;
        }
    }

    // 评分高的排前面；同分时保持 UTF-8 优先（最常见的情况）
    std::stable_sort(result.begin(), result.end(),
                     [](const Candidate &a, const Candidate &b) {
                         return a.score > b.score;
                     });
    return result;
}

int EncodingFixer::readabilityScore(const QString &text, int *replacementChars)
{
    int replacements = 0;
    int score = 0;

    for (const QChar ch : text) {
        const ushort u = ch.unicode();

        if (u == 0xFFFD) {
            // 解码失败的替换字符：最强负分信号
            ++replacements;
            score -= 20;
            continue;
        }
        if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r')
            || ch == QLatin1Char('\t')) {
            score += 1;
            continue;
        }
        if (u < 0x20 || u == 0x7F) {
            // 控制字符：正常文本里不该出现
            score -= 8;
            continue;
        }
        if (u >= 0xE000 && u <= 0xF8FF) {
            // Unicode 私用区
            score -= 8;
            continue;
        }
        if (u >= 0xD800 && u <= 0xDFFF) {
            // 孤立代理项
            score -= 8;
            continue;
        }
        if ((u >= 0x0370 && u <= 0x03FF)   // 希腊字母
            || (u >= 0x0400 && u <= 0x04FF) // 西里尔字母
            || (u >= 0x0530 && u <= 0x058F)) { // 亚美尼亚字母
            // 编码不匹配最典型的症状：GBK 字节被 UTF-8 解读后
            // 恰好构成合法的多字节序列，于是出现希腊/西里尔字母，
            // 编码器不报错但内容完全不是原文。
            score -= 6;
            continue;
        }
        if (u >= 0x3400 && u <= 0x4DBF) {
            // CJK 扩展 A：合法但罕见，扣一点分
            score -= 2;
            continue;
        }
        if (QChar::category(u) == QChar::Other_NotAssigned) {
            // 未分配码位：多见于「用错编码」造成的畸形结果
            score -= 6;
            continue;
        }
        if (u < 0x80) {
            score += 2; // 可打印 ASCII
            continue;
        }
        if (u >= 0x4E00 && u <= 0x9FFF) {
            score += 3; // CJK 统一汉字：串口设备最常见的中文
            continue;
        }
        score += 2; // 其余可打印字符（标点、全角符号等）
    }

    if (replacementChars)
        *replacementChars = replacements;
    return score;
}
