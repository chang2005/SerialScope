#pragma once

#include <QByteArray>
#include <QString>
#include <QTextCodec>

/// 文本编码选项
enum class TextEncoding {
    Ascii,
    Utf8,
    Gbk,
};

QString textEncodingName(TextEncoding encoding);
TextEncoding textEncodingFromName(const QString &name);

/// HEX / 文本编解码工具（无 UI 依赖）
class HexCodec
{
public:
    /// 解析用户输入的 HEX 字符串。
    /// 支持 "AA 55 01"、"AA5501"、"0xAA, 0x55"、跨行输入。
    /// 失败时返回空数组并在 error 中给出原因。
    static QByteArray parseHex(const QString &text, QString *error = nullptr);

    /// 格式化为 "AA 55 01"；bytesPerLine > 0 时按行切分。
    static QString formatHex(const QByteArray &data, int bytesPerLine = 0);

    /// 字节 → 文本。state 非空时支持跨数据块的增量解码（UTF-8 / GBK 半字符）。
    static QString decode(const QByteArray &data, TextEncoding encoding,
                          QTextCodec::ConverterState *state = nullptr);

    /// 文本 → 字节。
    static QByteArray encode(const QString &text, TextEncoding encoding);

    static QTextCodec *codecFor(TextEncoding encoding);
};
