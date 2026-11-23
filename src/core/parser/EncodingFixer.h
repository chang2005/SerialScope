#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

/// 编码转换与乱码修复（无 UI 依赖）
///
/// 乱码的典型成因是"用错误的编码解读了字节"：
///
///     设备发 GBK 字节 -> 工具按 UTF-8 解码 -> 出现替换字符
///     设备发 UTF-8 字节 -> 工具按 GBK 解码 -> 出现「涓枃」这类怪字符
///
/// 修复思路是"退回字节再换编码重解"：
///
///     bytes = encode(乱码文本, 错误编码)
///     text  = decode(bytes, 正确编码)
///
/// 第一种情况（出现 U+FFFD 替换字符）字节已丢失，只能靠原始字节重新判断，
/// 此时走 EncodingFixer::candidates()，用原始 HEX 字节逐个编码试解。
class EncodingFixer
{
public:
    /// 可参与转换的编码
    enum class Encoding {
        Latin1, ///< ISO-8859-1，字节与字符一一对应，可无损往返
        Utf8,   ///< UTF-8
        Gbk,    ///< GBK / CP936
        Ascii,  ///< 纯 ASCII（仅可打印字符）
    };

    /// 一种编码下的解码结果及其可读性评估
    struct Candidate
    {
        Encoding encoding = Encoding::Utf8;
        QString text;             ///< 解码结果
        int replacementChars = 0; ///< 无法解码的替换字符数量（U+FFFD）
        int score = 0;            ///< 可读性评分，越高越像正常文本

        /// 原始字节是否构成合法的 UTF-8 序列（用于区分 UTF-8 与 GBK）
        bool bytesAreValidUtf8 = false;

        /// 是否为一次无损解码（没有任何字节被丢弃）
        bool isLossless() const { return replacementChars == 0; }
    };

    /// 编码显示名
    static QString encodingName(Encoding encoding);

    /// 按指定编码把文本转成字节
    static QByteArray encodeText(const QString &text, Encoding encoding);

    /// 按指定编码把字节解读成文本
    static QString decodeBytes(const QByteArray &bytes, Encoding encoding);

    /// 把乱码文本从 src 编码逆转回字节，再用 dst 编码重新解读
    static QString fix(const QString &garbled, Encoding src, Encoding dst);

    /// 同一段字节分别用各编码解码，给出全部候选及其评分（按评分从高到低）
    static QVector<Candidate> candidates(const QByteArray &bytes);

    /// 只用指定编码解码，返回候选信息
    static Candidate decodeWith(const QByteArray &bytes, Encoding encoding);

    /// 严格校验字节序列是否为合法 UTF-8
    ///
    /// 这是区分 UTF-8 与 GBK 的关键：UTF-8 自描述，字节要么完全合法、要么立刻报错；
    /// GBK 的中文字节对（B4 AE 之类）不是合法 UTF-8。
    static bool isValidUtf8(const QByteArray &bytes);

    /// 可读性评分：替换字符与控制字符扣分，可打印字符加分
    static int readabilityScore(const QString &text, int *replacementChars = nullptr);
};
