#include "core/AutoReply.h"
#include "core/QuickSend.h"
#include "core/SequenceSender.h"
#include "core/parser/AsciiValueParser.h"
#include "core/parser/ControlCharVisualizer.h"
#include "core/parser/DataFrameParser.h"
#include "core/parser/EncodingFixer.h"
#include "core/parser/HexCodec.h"
#include "core/parser/HexFieldExtractor.h"
#include "core/parser/TextFilter.h"
#include "utils/CrcCalculator.h"

#include <QTest>
#include <QTextCodec>

#include <cmath>

class TestCore : public QObject
{
    Q_OBJECT

private slots:
    void crcVectors();
    void crcAppendMode();
    void crcLrcVector();

    void hexParsing();
    void hexFormatting();
    void hexDecodeAscii();
    void hexDecodeUtf8Incremental();
    void hexGbkRoundTrip();

    void encodingFixUtf8ToGbk();
    void encodingFixLatin1RoundTrip();
    void encodingCandidatesRankByReadability();
    void encodingCandidatesPreferUtf8ForUtf8Bytes();
    void encodingValidUtf8RejectsGbkBytes();
    void encodingReadabilityPenalizesGarbage();
    void encodingFixDetectsUtf8BytesAsGbk();

    void asciiParserBasic();
    void asciiParserSplitLine();
    void asciiParserSkipsLabelsAndHex();
    void asciiParserMaxChannels();
    void asciiParserOverflow();

    void frameParserLengthByteSplit();
    void frameParserSkipsNoise();
    void frameParserPartialHeaderAcrossChunks();
    void frameParserCrcFixedWithHeader();
    void frameParserCrcFixedNoHeader();
    void frameParserLrcChecksum();
    void frameParserXorChecksum();
    void frameParserCcittChecksum();
    void frameParserFixedTail();
    void frameParserTailMismatch();

    void fieldExtractorLittleEndian();
    void fieldExtractorBigEndian();
    void fieldExtractorOutOfRange();

    void filterInactiveMatchesEverything();
    void filterPlainTextCaseInsensitive();
    void filterMultipleOccurrences();
    void filterRegexMode();
    void filterInvalidRegexFallsBack();
    void filterHighlightMarkers();

    void controlCharsEscapeCommon();
    void controlCharsEscapeAll();
    void controlCharsPlainTextUnchanged();

    void quickSendAddAndFind();
    void quickSendOverwriteSameName();
    void quickSendRemove();
    void quickSendJsonRoundTrip();

    void autoReplyMatchesPlainText();
    void autoReplyRegex();
    void autoReplyFirstMatchWins();
    void autoReplyEscapes();
    void autoReplyHexPayload();
    void autoReplyJsonRoundTrip();

    void sequenceSetAddUpdateRemove();
    void sequenceSetRejectsEmptyGroup();
    void sequenceSetJsonRoundTrip();
    void sequenceRunnerWalksItemsThenGroups();
    void sequenceRunnerLoopsBackToFirstGroup();
    void sequenceRunnerSkipsEmptyGroups();
    void sequenceRunnerStartFailsWhenEmpty();
};

void TestCore::crcVectors()
{
    const QByteArray check = QByteArrayLiteral("123456789");
    QCOMPARE(static_cast<int>(CrcCalculator::crc8(check)), 0xF4);
    QCOMPARE(static_cast<int>(CrcCalculator::crc16Modbus(check)), 0x4B37);
    QCOMPARE(static_cast<int>(CrcCalculator::crc16CcittFalse(check)), 0x29B1);
    QCOMPARE(static_cast<quint32>(CrcCalculator::crc32(check)), quint32(0xCBF43926));
    QCOMPARE(static_cast<int>(CrcCalculator::sum8(check)), 0xDD);
    QCOMPARE(static_cast<int>(CrcCalculator::xor8(check)), 0x31);
}

void TestCore::crcAppendMode()
{
    const QByteArray payload = QByteArray::fromHex("010300000002");
    const quint16 modbus = CrcCalculator::crc16Modbus(payload);
    const QByteArray withModbus =
        CrcCalculator::appendChecksum(payload, CrcCalculator::AppendMode::Crc16Modbus);
    QCOMPARE(withModbus.size(), payload.size() + 2);
    QCOMPARE(withModbus.left(payload.size()), payload);
    QCOMPARE(static_cast<quint8>(withModbus.at(payload.size())),
             static_cast<quint8>(modbus & 0xFF));
    QCOMPARE(static_cast<quint8>(withModbus.at(payload.size() + 1)),
             static_cast<quint8>((modbus >> 8) & 0xFF));

    const quint16 ccitt = CrcCalculator::crc16CcittFalse(payload);
    const QByteArray withCcitt =
        CrcCalculator::appendChecksum(payload, CrcCalculator::AppendMode::Crc16Ccitt);
    QCOMPARE(withCcitt.size(), payload.size() + 2);
    QCOMPARE(static_cast<quint8>(withCcitt.at(payload.size())),
             static_cast<quint8>((ccitt >> 8) & 0xFF));
    QCOMPARE(static_cast<quint8>(withCcitt.at(payload.size() + 1)),
             static_cast<quint8>(ccitt & 0xFF));

    const QByteArray withSum =
        CrcCalculator::appendChecksum(payload, CrcCalculator::AppendMode::Sum8);
    QCOMPARE(withSum.size(), payload.size() + 1);
    QCOMPARE(static_cast<quint8>(withSum.back()),
             CrcCalculator::sum8(payload));

    const QByteArray withXor =
        CrcCalculator::appendChecksum(payload, CrcCalculator::AppendMode::Xor8);
    QCOMPARE(withXor.size(), payload.size() + 1);
    QCOMPARE(static_cast<quint8>(withXor.back()),
             CrcCalculator::xor8(payload));

    const QByteArray withLrc =
        CrcCalculator::appendChecksum(payload, CrcCalculator::AppendMode::Lrc8);
    QCOMPARE(withLrc.size(), payload.size() + 1);
    QCOMPARE(static_cast<quint8>(withLrc.back()),
             CrcCalculator::lrc8(payload));

    const QByteArray unchanged =
        CrcCalculator::appendChecksum(payload, CrcCalculator::AppendMode::None);
    QCOMPARE(unchanged, payload);
}

void TestCore::crcLrcVector()
{
    // LRC = 各字节求和后取补码（Modbus ASCII）
    const QByteArray check = QByteArrayLiteral("123456789");
    const quint32 sum = 0x31 + 0x32 + 0x33 + 0x34 + 0x35 + 0x36 + 0x37 + 0x38 + 0x39;
    QCOMPARE(static_cast<int>(CrcCalculator::lrc8(check)),
             static_cast<int>((~sum + 1) & 0xFF));

    // 空数据的 LRC 为 0
    QCOMPARE(static_cast<int>(CrcCalculator::lrc8(QByteArray())), 0x00);
}

void TestCore::hexParsing()
{
    QString error;
    QCOMPARE(HexCodec::parseHex(QStringLiteral("AA 55 01"), &error),
             QByteArray::fromHex("AA5501"));
    QVERIFY(error.isEmpty());

    QCOMPARE(HexCodec::parseHex(QStringLiteral("0xAA, 0x55;\n0x01"), &error),
             QByteArray::fromHex("AA5501"));
    QVERIFY(error.isEmpty());

    QCOMPARE(HexCodec::parseHex(QStringLiteral("AA_55_01"), &error),
             QByteArray::fromHex("AA5501"));
    QVERIFY(error.isEmpty());

    QVERIFY(HexCodec::parseHex(QStringLiteral("AA5"), &error).isEmpty());
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(HexCodec::parseHex(QStringLiteral("GG"), &error).isEmpty());
    QVERIFY(!error.isEmpty());

    QVERIFY(HexCodec::parseHex(QString(), &error).isEmpty());
    QVERIFY(error.isEmpty());
}

void TestCore::hexFormatting()
{
    const QByteArray data = QByteArray::fromHex("AA5501");
    QCOMPARE(HexCodec::formatHex(data), QStringLiteral("AA 55 01"));
    QCOMPARE(HexCodec::formatHex(data, 2), QStringLiteral("AA 55\n01"));
    QCOMPARE(HexCodec::formatHex(QByteArray()), QString());
}

void TestCore::hexDecodeAscii()
{
    const QByteArray data("A\x01" "B", 3);
    QCOMPARE(HexCodec::decode(data, TextEncoding::Ascii), QStringLiteral("A·B"));
    QCOMPARE(HexCodec::encode(QStringLiteral("A中"), TextEncoding::Ascii),
             QByteArrayLiteral("A?"));
}

void TestCore::hexDecodeUtf8Incremental()
{
    const QByteArray encoded = QStringLiteral("中").toUtf8();
    QCOMPARE(encoded.size(), 3);

    QTextCodec::ConverterState state;
    const QString first = HexCodec::decode(encoded.left(1), TextEncoding::Utf8, &state);
    QVERIFY2(first.isEmpty(), "不完整的多字节字符不应产生输出");

    const QString rest = HexCodec::decode(encoded.mid(1), TextEncoding::Utf8, &state);
    QCOMPARE(rest, QStringLiteral("中"));

    QCOMPARE(HexCodec::encode(QStringLiteral("中文"), TextEncoding::Utf8),
             QStringLiteral("中文").toUtf8());
}

void TestCore::hexGbkRoundTrip()
{
    if (!HexCodec::codecFor(TextEncoding::Gbk))
        QSKIP("当前 Qt 构建不包含 GBK 编解码器");

    const QByteArray gbk = HexCodec::encode(QStringLiteral("串口调试"), TextEncoding::Gbk);
    QVERIFY(!gbk.isEmpty());
    QCOMPARE(HexCodec::decode(gbk, TextEncoding::Gbk), QStringLiteral("串口调试"));

    QTextCodec::ConverterState state;
    const QString first = HexCodec::decode(gbk.left(1), TextEncoding::Gbk, &state);
    QVERIFY(first.isEmpty());
    QCOMPARE(HexCodec::decode(gbk.mid(1), TextEncoding::Gbk, &state),
             QStringLiteral("串口调试"));
}

void TestCore::encodingFixUtf8ToGbk()
{
    if (!HexCodec::codecFor(TextEncoding::Gbk))
        QSKIP("当前 Qt 构建不包含 GBK 编解码器");

    // 设备发 UTF-8 字节，工具按 GBK 解读 → 乱码；再用 GBK→UTF-8 转回来应还原
    const QString original = QStringLiteral("串口调试");
    const QByteArray utf8Bytes = original.toUtf8();
    const QString garbled = EncodingFixer::decodeBytes(utf8Bytes,
                                                       EncodingFixer::Encoding::Gbk);
    QVERIFY(garbled != original);

    const QString fixed = EncodingFixer::fix(garbled, EncodingFixer::Encoding::Gbk,
                                             EncodingFixer::Encoding::Utf8);
    QCOMPARE(fixed, original);
}

void TestCore::encodingFixLatin1RoundTrip()
{
    // Latin-1 与字节一一对应，必须能无损往返
    const QString text = QStringLiteral("ABC\r\n");
    const QByteArray bytes = EncodingFixer::encodeText(text, EncodingFixer::Encoding::Latin1);
    QCOMPARE(bytes.size(), text.size());
    QCOMPARE(EncodingFixer::decodeBytes(bytes, EncodingFixer::Encoding::Latin1), text);
    QCOMPARE(EncodingFixer::fix(text, EncodingFixer::Encoding::Latin1,
                                EncodingFixer::Encoding::Latin1),
             text);
}

void TestCore::encodingCandidatesRankByReadability()
{
    const QByteArray gbk = HexCodec::encode(QStringLiteral("串口调试"), TextEncoding::Gbk);
    if (gbk.isEmpty())
        QSKIP("当前环境不支持 GBK 编码");

    const QVector<EncodingFixer::Candidate> candidates =
        EncodingFixer::candidates(gbk);
    QCOMPARE(candidates.size(), 4);

    // 评分必须从高到低排列
    for (int i = 1; i < candidates.size(); ++i)
        QVERIFY(candidates.at(i - 1).score >= candidates.at(i).score);

    // 正确的 GBK 解码应排在第一，且没有替换字符
    QCOMPARE(candidates.first().encoding, EncodingFixer::Encoding::Gbk);
    QCOMPARE(candidates.first().replacementChars, 0);
    QCOMPARE(candidates.first().text, QStringLiteral("串口调试"));
}

void TestCore::encodingCandidatesPreferUtf8ForUtf8Bytes()
{
    const QByteArray utf8Bytes = QStringLiteral("串口调试").toUtf8();
    QVERIFY(EncodingFixer::isValidUtf8(utf8Bytes));

    const QVector<EncodingFixer::Candidate> candidates =
        EncodingFixer::candidates(utf8Bytes);
    QCOMPARE(candidates.size(), 4);
    QCOMPARE(candidates.first().encoding, EncodingFixer::Encoding::Utf8);
    QCOMPARE(candidates.first().text, QStringLiteral("串口调试"));
    QCOMPARE(candidates.first().replacementChars, 0);
}

void TestCore::encodingValidUtf8RejectsGbkBytes()
{
    // GBK 的中文字节对不是合法 UTF-8，这是区分两种编码的关键依据
    const QByteArray gbk = HexCodec::encode(QStringLiteral("串口调试"), TextEncoding::Gbk);
    if (gbk.isEmpty())
        QSKIP("当前环境不支持 GBK 编码");
    QVERIFY(!EncodingFixer::isValidUtf8(gbk));

    // 纯 ASCII 同时是合法的 UTF-8
    QVERIFY(EncodingFixer::isValidUtf8(QByteArray("hello world")));
    QVERIFY(EncodingFixer::isValidUtf8(QByteArray()));
    // 非法续字节
    QVERIFY(!EncodingFixer::isValidUtf8(QByteArray::fromHex("80")));
    // 过长编码
    QVERIFY(!EncodingFixer::isValidUtf8(QByteArray::fromHex("C0AF")));
    // 截断的多字节序列
    QVERIFY(!EncodingFixer::isValidUtf8(QByteArray::fromHex("E4B8")));
}

void TestCore::encodingReadabilityPenalizesGarbage()
{
    int replacements = 0;
    const int good = EncodingFixer::readabilityScore(QStringLiteral("hello 中文"), &replacements);
    QCOMPARE(replacements, 0);

    const int bad = EncodingFixer::readabilityScore(QStringLiteral("���"), &replacements);
    QCOMPARE(replacements, 3);
    QVERIFY(bad < good);
}

void TestCore::encodingFixDetectsUtf8BytesAsGbk()
{
    // UTF-8 字节被当作 GBK 解读时会变成可打印但无意义的字符，
    // 换成 UTF-8 解读后应恢复正常且评分更高。
    const QByteArray utf8Bytes = QStringLiteral("温度25").toUtf8();
    const auto gbkCandidate = EncodingFixer::decodeWith(utf8Bytes, EncodingFixer::Encoding::Gbk);
    const auto utf8Candidate = EncodingFixer::decodeWith(utf8Bytes, EncodingFixer::Encoding::Utf8);
    QCOMPARE(utf8Candidate.text, QStringLiteral("温度25"));
    QVERIFY(utf8Candidate.replacementChars <= gbkCandidate.replacementChars);
}

void TestCore::asciiParserBasic()
{
    AsciiValueParser parser;
    QVector<QVector<double>> rows;
    parser.onValues = [&rows](const QVector<double> &values) { rows.append(values); };

    parser.feed(QByteArrayLiteral("1.5, -2.0, 3e2\r\n"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).size(), 3);
    QCOMPARE(rows.at(0).at(0), 1.5);
    QCOMPARE(rows.at(0).at(1), -2.0);
    QCOMPARE(rows.at(0).at(2), 300.0);

    parser.feed(QByteArrayLiteral("10 20\n30 40\n"));
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(1).size(), 2);
    QCOMPARE(rows.at(1).at(1), 20.0);
}

void TestCore::asciiParserSplitLine()
{
    AsciiValueParser parser;
    QVector<QVector<double>> rows;
    parser.onValues = [&rows](const QVector<double> &values) { rows.append(values); };

    parser.feed(QByteArrayLiteral("12"));
    QCOMPARE(rows.size(), 0);
    parser.feed(QByteArrayLiteral(".5\n"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).size(), 1);
    QCOMPARE(rows.at(0).at(0), 12.5);

    parser.feed(QByteArrayLiteral("-"));
    parser.feed(QByteArrayLiteral("7\r"));
    QCOMPARE(rows.size(), 2);
    QCOMPARE(rows.at(1).at(0), -7.0);
}

void TestCore::asciiParserSkipsLabelsAndHex()
{
    AsciiValueParser parser;
    QVector<QVector<double>> rows;
    parser.onValues = [&rows](const QVector<double> &values) { rows.append(values); };

    parser.feed(QByteArrayLiteral("CH1: 99\n"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).size(), 1);
    QCOMPARE(rows.at(0).at(0), 99.0);

    rows.clear();
    parser.feed(QByteArrayLiteral("0x1F 42\n"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).size(), 1);
    QCOMPARE(rows.at(0).at(0), 42.0);
}

void TestCore::asciiParserMaxChannels()
{
    AsciiValueParser parser;
    parser.setMaxChannels(3);
    QVector<QVector<double>> rows;
    parser.onValues = [&rows](const QVector<double> &values) { rows.append(values); };

    parser.feed(QByteArrayLiteral("1 2 3 4 5\n"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).size(), 3);
    QCOMPARE(rows.at(0).at(2), 3.0);
}

void TestCore::asciiParserOverflow()
{
    AsciiValueParser parser;
    int overflowCount = 0;
    parser.onOverflow = [&overflowCount]() { ++overflowCount; };

    QByteArray flood(70 * 1024, '1');
    parser.feed(flood);
    QCOMPARE(overflowCount, 1);

    QVector<QVector<double>> rows;
    parser.onValues = [&rows](const QVector<double> &values) { rows.append(values); };
    parser.feed(QByteArrayLiteral("5\n"));
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).at(0), 5.0);
}

void TestCore::frameParserLengthByteSplit()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::LengthByte;
    spec.lengthByteOffset = 2;
    spec.lengthAdjust = 6;
    spec.maxFrameLength = 256;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };

    const QByteArray frame = QByteArray::fromHex("AA550401020304050607");
    QCOMPARE(frame.size(), 10);

    parser.feed(frame.left(1));
    QCOMPARE(frames.size(), 0);
    parser.feed(frame.mid(1, 4));
    QCOMPARE(frames.size(), 0);
    parser.feed(frame.mid(5));
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.at(0), frame);

    // 粘包：一次喂入两帧
    frames.clear();
    parser.feed(frame + frame);
    QCOMPARE(frames.size(), 2);
}

void TestCore::frameParserSkipsNoise()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 6;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    int discarded = 0;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };
    parser.onBytesDiscarded = [&discarded](int bytes) { discarded += bytes; };

    parser.feed(QByteArray::fromHex("00FFAA5501020304"));
    QCOMPARE(frames.size(), 1);
    QCOMPARE(discarded, 2);
    QCOMPARE(frames.at(0), QByteArray::fromHex("AA5501020304"));
}

void TestCore::frameParserPartialHeaderAcrossChunks()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 4;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    int discarded = 0;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };
    parser.onBytesDiscarded = [&discarded](int bytes) { discarded += bytes; };

    parser.feed(QByteArray::fromHex("00AA"));
    QCOMPARE(frames.size(), 0);
    QCOMPARE(discarded, 1); // 只丢弃 00，保留半个帧头 AA

    parser.feed(QByteArray::fromHex("550102"));
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.at(0), QByteArray::fromHex("AA550102"));
}

void TestCore::frameParserCrcFixedWithHeader()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 8;
    spec.checksum = FrameChecksum::Crc16Modbus;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    int discarded = 0;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };
    parser.onBytesDiscarded = [&discarded](int bytes) { discarded += bytes; };

    const QByteArray payload = QByteArray::fromHex("AA5501020304");
    const quint16 crc = CrcCalculator::crc16Modbus(payload);
    QByteArray good = payload;
    good.append(static_cast<char>(crc & 0xFF));
    good.append(static_cast<char>((crc >> 8) & 0xFF));
    QCOMPARE(good.size(), 8);

    parser.feed(good);
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.at(0), good);

    QByteArray bad = good;
    bad[3] = static_cast<char>(bad.at(3) ^ 0x5A);
    parser.feed(bad);
    QCOMPARE(frames.size(), 1);
    QVERIFY(discarded > 0);
}

void TestCore::frameParserCrcFixedNoHeader()
{
    FrameSpec spec;
    spec.header.clear();
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 8;
    spec.checksum = FrameChecksum::Crc16Modbus;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    int discarded = 0;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };
    parser.onBytesDiscarded = [&discarded](int bytes) { discarded += bytes; };

    const QByteArray payload = QByteArray::fromHex("010203040506");
    const quint16 crc = CrcCalculator::crc16Modbus(payload);
    QByteArray good = payload;
    good.append(static_cast<char>(crc & 0xFF));
    good.append(static_cast<char>((crc >> 8) & 0xFF));

    parser.feed(good);
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.at(0), good);

    QByteArray bad = good;
    bad[0] = static_cast<char>(bad.at(0) ^ 0xFF);
    parser.feed(bad);
    QCOMPARE(frames.size(), 1);
    QVERIFY(discarded > 0);
}

void TestCore::frameParserLrcChecksum()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 6; // 帧头 2 + 数据 3 + LRC 1
    spec.checksum = FrameChecksum::Lrc8;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    int discarded = 0;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };
    parser.onBytesDiscarded = [&discarded](int bytes) { discarded += bytes; };

    const QByteArray payload = QByteArray::fromHex("AA55010203");
    QByteArray good = payload;
    good.append(static_cast<char>(CrcCalculator::lrc8(payload)));
    QCOMPARE(good.size(), 6);

    parser.feed(good);
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.at(0), good);

    QByteArray bad = good;
    bad[3] = static_cast<char>(bad.at(3) ^ 0x33);
    parser.feed(bad);
    QCOMPARE(frames.size(), 1);
    QVERIFY(discarded > 0);
}

void TestCore::frameParserXorChecksum()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 6; // 帧头 2 + 数据 3 + Xor 1
    spec.checksum = FrameChecksum::Xor8;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };

    const QByteArray payload = QByteArray::fromHex("AA55010203");
    QByteArray good = payload;
    good.append(static_cast<char>(CrcCalculator::xor8(payload)));

    parser.feed(good);
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.at(0), good);
}

void TestCore::frameParserCcittChecksum()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 8;
    spec.checksum = FrameChecksum::Crc16Ccitt;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };

    const QByteArray payload = QByteArray::fromHex("AA5501020304");
    const quint16 crc = CrcCalculator::crc16CcittFalse(payload);
    QByteArray good = payload;
    good.append(static_cast<char>((crc >> 8) & 0xFF)); // CCITT 高字节在前
    good.append(static_cast<char>(crc & 0xFF));

    parser.feed(good);
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.at(0), good);

    // 小端排列不应被接受
    QByteArray littleEndian = payload;
    littleEndian.append(static_cast<char>(crc & 0xFF));
    littleEndian.append(static_cast<char>((crc >> 8) & 0xFF));
    parser.feed(littleEndian);
    QCOMPARE(frames.size(), 1);
}

void TestCore::frameParserFixedTail()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 9; // 帧头 2 + 数据 3 + CRC16 2 + 帧尾 2
    spec.checksum = FrameChecksum::Crc16Modbus;
    spec.tail = QByteArray::fromHex("0D0A");
    spec.tailMode = FrameTailMode::Fixed;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };

    // 帧内结构：[帧头][数据][校验][帧尾]
    const QByteArray payload = QByteArray::fromHex("AA55010203");
    const quint16 crc = CrcCalculator::crc16Modbus(payload);
    QByteArray good = payload;
    good.append(static_cast<char>(crc & 0xFF));
    good.append(static_cast<char>((crc >> 8) & 0xFF));
    good.append(QByteArray::fromHex("0D0A"));
    QCOMPARE(good.size(), 9);

    parser.feed(good);
    QCOMPARE(frames.size(), 1);
    QCOMPARE(frames.at(0), good);
}

void TestCore::frameParserTailMismatch()
{
    FrameSpec spec;
    spec.header = QByteArray::fromHex("AA55");
    spec.lengthMode = FrameSpec::LengthMode::Fixed;
    spec.fixedFrameLength = 9; // 帧头 2 + 数据 3 + CRC16 2 + 帧尾 2
    spec.checksum = FrameChecksum::Crc16Modbus;
    spec.tail = QByteArray::fromHex("0D0A");
    spec.tailMode = FrameTailMode::Fixed;

    DataFrameParser parser;
    parser.setSpec(spec);

    QVector<QByteArray> frames;
    int discarded = 0;
    parser.onFrame = [&frames](const QByteArray &frame) { frames.append(frame); };
    parser.onBytesDiscarded = [&discarded](int bytes) { discarded += bytes; };

    const QByteArray payload = QByteArray::fromHex("AA55010203");
    const quint16 crc = CrcCalculator::crc16Modbus(payload);
    QByteArray bad = payload;
    bad.append(static_cast<char>(crc & 0xFF));
    bad.append(static_cast<char>((crc >> 8) & 0xFF));
    bad.append(QByteArray::fromHex("0A0D")); // 帧尾字节顺序反了

    parser.feed(bad);
    QCOMPARE(frames.size(), 0);
    QVERIFY(discarded > 0);
}

void TestCore::fieldExtractorLittleEndian()
{
    const QByteArray frame = QByteArray::fromHex("2AFF34120000C03F")
        + QByteArray(4, '\xFF');
    QCOMPARE(frame.size(), 12);

    HexParseSpec spec;
    spec.bigEndian = false;
    spec.fields = {
        {QStringLiteral("raw"), 0, FieldType::UInt8},
        {QStringLiteral("i8"), 1, FieldType::Int8},
        {QStringLiteral("u16"), 2, FieldType::UInt16},
        {QStringLiteral("f32"), 4, FieldType::Float32},
        {QStringLiteral("i32"), 8, FieldType::Int32},
        {QStringLiteral("u32"), 8, FieldType::UInt32},
    };

    bool ok = false;
    const QVector<double> values = HexFieldExtractor::extract(frame, spec, &ok);
    QVERIFY(ok);
    QCOMPARE(values.size(), 6);
    QCOMPARE(values.at(0), 42.0);
    QCOMPARE(values.at(1), -1.0);
    QCOMPARE(values.at(2), 4660.0);
    QCOMPARE(values.at(3), 1.5);
    QCOMPARE(values.at(4), -1.0);
    QCOMPARE(values.at(5), 4294967295.0);
}

void TestCore::fieldExtractorBigEndian()
{
    const QByteArray frame = QByteArray::fromHex("2AFF12343FC00000FFFFFFFF");
    QCOMPARE(frame.size(), 12);

    HexParseSpec spec;
    spec.bigEndian = true;
    spec.fields = {
        {QStringLiteral("u16"), 2, FieldType::UInt16},
        {QStringLiteral("f32"), 4, FieldType::Float32},
        {QStringLiteral("u32"), 8, FieldType::UInt32},
    };

    bool ok = false;
    const QVector<double> values = HexFieldExtractor::extract(frame, spec, &ok);
    QVERIFY(ok);
    QCOMPARE(values.at(0), 4660.0);
    QCOMPARE(values.at(1), 1.5);
    QCOMPARE(values.at(2), 4294967295.0);
}

void TestCore::fieldExtractorOutOfRange()
{
    const QByteArray frame = QByteArray::fromHex("01020304");
    HexParseSpec spec;
    spec.fields = {
        {QStringLiteral("ok"), 0, FieldType::UInt16},
        {QStringLiteral("cross"), 3, FieldType::UInt16},
        {QStringLiteral("far"), 100, FieldType::UInt32},
        {QStringLiteral("negative"), -1, FieldType::UInt8},
    };

    bool ok = true;
    const QVector<double> values = HexFieldExtractor::extract(frame, spec, &ok);
    QVERIFY(!ok);
    QCOMPARE(values.size(), 4);
    QCOMPARE(values.at(0), 513.0);
    QVERIFY(std::isnan(values.at(1)));
    QVERIFY(std::isnan(values.at(2)));
    QVERIFY(std::isnan(values.at(3)));
}

// ---------------------------------------------------------------- 文本过滤

void TestCore::filterInactiveMatchesEverything()
{
    TextFilter filter;
    QVERIFY(!filter.isActive());
    QVERIFY(filter.matchesLine(QStringLiteral("anything")));

    filter.setPattern(QString(), false);
    QVERIFY(!filter.isActive());
    QVERIFY(filter.matchesLine(QStringLiteral("anything")));
}

void TestCore::filterPlainTextCaseInsensitive()
{
    TextFilter filter;
    QVERIFY(filter.setPattern(QStringLiteral("err"), false));

    const QList<QPair<int, int>> ranges =
        filter.matchRanges(QStringLiteral("ERR value"));
    QCOMPARE(ranges.size(), 1);
    QCOMPARE(ranges.first().first, 0);
    QCOMPARE(ranges.first().second, 3);

    QVERIFY(filter.matchesLine(QStringLiteral("some ERROR here")));
    QVERIFY(!filter.matchesLine(QStringLiteral("all good")));
}

void TestCore::filterMultipleOccurrences()
{
    TextFilter filter;
    filter.setPattern(QStringLiteral("ab"), false);

    const QList<QPair<int, int>> ranges =
        filter.matchRanges(QStringLiteral("abXXab"));
    QCOMPARE(ranges.size(), 2);
    QCOMPARE(ranges.at(0).first, 0);
    QCOMPARE(ranges.at(0).second, 2);
    QCOMPARE(ranges.at(1).first, 4);
    QCOMPARE(ranges.at(1).second, 6);
}

void TestCore::filterRegexMode()
{
    TextFilter filter;
    QVERIFY(filter.setPattern(QStringLiteral("t\\d+"), true));

    const QList<QPair<int, int>> ranges =
        filter.matchRanges(QStringLiteral("t12 and t345"));
    QCOMPARE(ranges.size(), 2);
    QCOMPARE(ranges.at(0).first, 0);
    QCOMPARE(ranges.at(0).second, 3);
    QCOMPARE(ranges.at(1).first, 8);
    QCOMPARE(ranges.at(1).second, 12);
}

void TestCore::filterInvalidRegexFallsBack()
{
    TextFilter filter;
    // 非法正则：setPattern 返回 false，过滤器退回普通文本模式
    QVERIFY(!filter.setPattern(QStringLiteral("[unclosed"), true));
    QVERIFY(filter.isActive());

    // 退回后按纯文本匹配 "[unclosed"
    const QList<QPair<int, int>> ranges =
        filter.matchRanges(QStringLiteral("x [unclosed y"));
    QCOMPARE(ranges.size(), 1);
    QCOMPARE(ranges.first().first, 2);
    QCOMPARE(ranges.first().second, 11);
}

void TestCore::filterHighlightMarkers()
{
    TextFilter filter;
    filter.setPattern(QStringLiteral("ok"), false);

    const QList<QPair<int, int>> ranges =
        filter.matchRanges(QStringLiteral("say ok now"));
    const QString marked = TextFilter::highlightMarkers(QStringLiteral("say ok now"), ranges);
    QCOMPARE(marked, QStringLiteral("say <mark>ok</mark> now"));
}

// ------------------------------------------------------------ 控制字符转义

void TestCore::controlCharsEscapeCommon()
{
    ControlCharVisualizer visualizer(false);

    QCOMPARE(visualizer.escape(QStringLiteral("a\r\nb")),
             QStringLiteral("a<CR><LF>b"));
    QCOMPARE(visualizer.escape(QStringLiteral("x\ty")),
             QStringLiteral("x<TAB>y"));
}

void TestCore::controlCharsEscapeAll()
{
    ControlCharVisualizer visualizer(true);

    QCOMPARE(visualizer.escape(QStringLiteral("\x01")),
             QStringLiteral("<0x01>"));
    QCOMPARE(visualizer.escape(QStringLiteral("\x7F")),
             QStringLiteral("<DEL>"));
    // 常见字符在 showAll 模式下同样保持一致
    QCOMPARE(visualizer.escape(QStringLiteral("\r")), QStringLiteral("<CR>"));
}

void TestCore::controlCharsPlainTextUnchanged()
{
    ControlCharVisualizer visualizer(true);

    const QString plain = QStringLiteral("hello world");
    QCOMPARE(visualizer.escape(plain), plain);
    QCOMPARE(visualizer.escape(QString()), QString());
}

// ------------------------------------------------------------ 快捷发送清单

void TestCore::quickSendAddAndFind()
{
    QuickSendList list;
    QVERIFY(list.isEmpty());

    QuickSendItem item;
    item.name = QStringLiteral("PING");
    item.payload = QStringLiteral("AT+PING");
    QVERIFY(list.add(item));
    QCOMPARE(list.count(), 1);

    const QuickSendItem *found = list.find(QStringLiteral("PING"));
    QVERIFY(found != nullptr);
    QCOMPARE(found->payload, QStringLiteral("AT+PING"));

    // 空负载不加入
    QuickSendItem empty;
    empty.name = QStringLiteral("X");
    QVERIFY(!list.add(empty));

    // 名称缺省时用负载前 12 字符
    QuickSendItem noName;
    noName.payload = QStringLiteral("ABCDEFGHIJKLMNOP");
    QVERIFY(list.add(noName));
    QVERIFY(list.find(QStringLiteral("ABCDEFGHIJKL")) != nullptr);
}

void TestCore::quickSendOverwriteSameName()
{
    QuickSendList list;
    QuickSendItem item;
    item.name = QStringLiteral("A");
    item.payload = QStringLiteral("one");
    QVERIFY(list.add(item));

    item.payload = QStringLiteral("two");
    QVERIFY(list.add(item));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list.find(QStringLiteral("A"))->payload, QStringLiteral("two"));
}

void TestCore::quickSendRemove()
{
    QuickSendList list;
    QuickSendItem item;
    item.name = QStringLiteral("A");
    item.payload = QStringLiteral("one");
    QVERIFY(list.add(item));

    QVERIFY(list.remove(QStringLiteral("A")));
    QCOMPARE(list.count(), 0);
    QVERIFY(!list.remove(QStringLiteral("A")));
}

void TestCore::quickSendJsonRoundTrip()
{
    QuickSendList list;
    QuickSendItem a;
    a.name = QStringLiteral("PING");
    a.payload = QStringLiteral("AT+PING");
    list.add(a);
    QuickSendItem b;
    b.name = QStringLiteral("RESET");
    b.payload = QStringLiteral("AA 55 01");
    b.hex = true;
    list.add(b);

    const QString json = list.toJson();
    QuickSendList restored;
    QVERIFY(restored.fromJson(json));
    QCOMPARE(restored.count(), 2);
    QCOMPARE(restored.find(QStringLiteral("RESET"))->hex, true);
    QCOMPARE(restored.find(QStringLiteral("PING"))->payload, QStringLiteral("AT+PING"));

    // 非法 JSON 不改变内容
    QuickSendList other;
    QVERIFY(!other.fromJson(QStringLiteral("{not json")));
    QVERIFY(other.isEmpty());
}

// -------------------------------------------------------------- 自动回复

void TestCore::autoReplyMatchesPlainText()
{
    AutoReplyEngine engine;
    AutoReplyRule rule;
    rule.name = QStringLiteral("ping");
    rule.matchPattern = QStringLiteral("PING");
    rule.replyPayload = QStringLiteral("PONG");
    QVERIFY(engine.addRule(rule));

    AutoReplyRule found;
    QByteArray reply;
    QVERIFY(engine.match(QByteArray("AT+PING\r\n"), &found, &reply));
    QCOMPARE(found.name, QStringLiteral("ping"));
    QCOMPARE(reply, QByteArray("PONG"));

    QVERIFY(!engine.match(QByteArray("AT+OTHER"), &found, &reply));
}

void TestCore::autoReplyRegex()
{
    AutoReplyEngine engine;
    AutoReplyRule rule;
    rule.name = QStringLiteral("cmd");
    rule.matchPattern = QStringLiteral("^AT\\+(\\w+)$");
    rule.replyPayload = QStringLiteral("OK");
    rule.useRegex = true;
    QVERIFY(engine.addRule(rule));

    QByteArray reply;
    QVERIFY(engine.match(QByteArray("AT+RESET"), nullptr, &reply));
    QCOMPARE(reply, QByteArray("OK"));
    QVERIFY(!engine.match(QByteArray("XX+RESET"), nullptr, &reply));
}

void TestCore::autoReplyFirstMatchWins()
{
    AutoReplyEngine engine;
    AutoReplyRule first;
    first.name = QStringLiteral("first");
    first.matchPattern = QStringLiteral("A");
    first.replyPayload = QStringLiteral("1");
    engine.addRule(first);

    AutoReplyRule second;
    second.name = QStringLiteral("second");
    second.matchPattern = QStringLiteral("A");
    second.replyPayload = QStringLiteral("2");
    engine.addRule(second);

    AutoReplyRule found;
    QByteArray reply;
    QVERIFY(engine.match(QByteArray("A"), &found, &reply));
    QCOMPARE(found.name, QStringLiteral("first"));
    QCOMPARE(reply, QByteArray("1"));
}

void TestCore::autoReplyEscapes()
{
    QCOMPARE(AutoReplyEngine::expandEscapes(QStringLiteral("a\\r\\nb")),
             QByteArray("a\r\nb"));
    QCOMPARE(AutoReplyEngine::expandEscapes(QStringLiteral("x\\ty")),
             QByteArray("x\ty"));
    QCOMPARE(AutoReplyEngine::expandEscapes(QStringLiteral("\\x41")),
             QByteArray("A"));
    QCOMPARE(AutoReplyEngine::expandEscapes(QStringLiteral("a\\\\b")),
             QByteArray("a\\b"));
}

void TestCore::autoReplyHexPayload()
{
    AutoReplyEngine engine;
    AutoReplyRule rule;
    rule.name = QStringLiteral("hex");
    rule.matchPattern = QStringLiteral("REQ");
    rule.replyPayload = QStringLiteral("AA 55 01");
    rule.replyIsHex = true;
    QVERIFY(engine.addRule(rule));

    QByteArray reply;
    QVERIFY(engine.match(QByteArray("REQ"), nullptr, &reply));
    QCOMPARE(reply, QByteArray::fromHex("AA5501"));
}

void TestCore::autoReplyJsonRoundTrip()
{
    AutoReplyEngine engine;
    AutoReplyRule rule;
    rule.name = QStringLiteral("ping");
    rule.matchPattern = QStringLiteral("PING");
    rule.replyPayload = QStringLiteral("PONG");
    rule.useRegex = false;
    engine.addRule(rule);

    const QString json = engine.toJson();
    AutoReplyEngine restored;
    QVERIFY(restored.fromJson(json));
    QCOMPARE(restored.count(), 1);
    QCOMPARE(restored.rules().first().replyPayload, QStringLiteral("PONG"));

    QVERIFY(!restored.fromJson(QStringLiteral("[]not json")));
}

void TestCore::sequenceSetAddUpdateRemove()
{
    SequenceSet set;
    SequenceGroup group;
    group.name = QStringLiteral("查询组");
    group.intervalMs = 500;
    SequenceItem item;
    item.label = QStringLiteral("读保持寄存器");
    item.payload = QByteArray::fromHex("010300000002");
    group.items.append(item);

    const int index = set.addGroup(group);
    QCOMPARE(index, 0);
    QCOMPARE(set.count(), 1);
    QCOMPARE(set.groupAt(0)->name, QStringLiteral("查询组"));
    QCOMPARE(set.groupAt(0)->intervalMs, 500);
    QCOMPARE(set.groupAt(0)->items.size(), 1);

    SequenceGroup updated = *set.groupAt(0);
    updated.intervalMs = 1500;
    QVERIFY(set.updateGroup(0, updated));
    QCOMPARE(set.groupAt(0)->intervalMs, 1500);

    QVERIFY(set.removeGroup(0));
    QCOMPARE(set.count(), 0);
    QVERIFY(!set.removeGroup(0));
    QVERIFY(!set.updateGroup(0, updated));
    QVERIFY(set.groupAt(0) == nullptr);
}

void TestCore::sequenceSetRejectsEmptyGroup()
{
    SequenceSet set;
    SequenceGroup empty;
    QCOMPARE(set.addGroup(empty), -1);
    QCOMPARE(set.count(), 0);
}

void TestCore::sequenceSetJsonRoundTrip()
{
    SequenceSet set;
    SequenceGroup group;
    group.name = QStringLiteral("组一");
    group.intervalMs = 800;
    SequenceItem a;
    a.label = QStringLiteral("A");
    a.payload = QByteArray::fromHex("AABB");
    SequenceItem b;
    b.label = QStringLiteral("B");
    b.payload = QByteArray::fromHex("CCDD");
    group.items.append(a);
    group.items.append(b);
    set.addGroup(group);

    const QString json = set.toJson();
    SequenceSet restored;
    QVERIFY(restored.fromJson(json));
    QCOMPARE(restored.count(), 1);
    const SequenceGroup *g = restored.groupAt(0);
    QVERIFY(g != nullptr);
    QCOMPARE(g->name, QStringLiteral("组一"));
    QCOMPARE(g->intervalMs, 800);
    QCOMPARE(g->items.size(), 2);
    QCOMPARE(g->items.at(0).payload, QByteArray::fromHex("AABB"));
    QCOMPARE(g->items.at(1).label, QStringLiteral("B"));

    QVERIFY(!restored.fromJson(QStringLiteral("not json")));
}

void TestCore::sequenceRunnerWalksItemsThenGroups()
{
    QVector<SequenceGroup> groups;
    SequenceGroup g1;
    g1.name = QStringLiteral("G1");
    SequenceItem i1;
    i1.payload = QByteArray::fromHex("01");
    SequenceItem i2;
    i2.payload = QByteArray::fromHex("02");
    g1.items.append(i1);
    g1.items.append(i2);

    SequenceGroup g2;
    g2.name = QStringLiteral("G2");
    SequenceItem i3;
    i3.payload = QByteArray::fromHex("03");
    g2.items.append(i3);
    groups.append(g1);
    groups.append(g2);

    SequenceRunner runner;
    runner.setGroups(groups);
    QVERIFY(runner.start());

    QByteArray payload;
    QString label;
    QVERIFY(runner.currentPayload(&payload, &label));
    QCOMPARE(payload, QByteArray::fromHex("01"));

    runner.advance();
    QVERIFY(runner.currentPayload(&payload, &label));
    QCOMPARE(payload, QByteArray::fromHex("02"));
    QCOMPARE(runner.currentGroupIndex(), 0);

    runner.advance();
    QVERIFY(runner.currentPayload(&payload, &label));
    QCOMPARE(payload, QByteArray::fromHex("03"));
    QCOMPARE(runner.currentGroupIndex(), 1);
}

void TestCore::sequenceRunnerLoopsBackToFirstGroup()
{
    QVector<SequenceGroup> groups;
    SequenceGroup g1;
    SequenceItem i1;
    i1.payload = QByteArray::fromHex("01");
    g1.items.append(i1);
    SequenceGroup g2;
    SequenceItem i2;
    i2.payload = QByteArray::fromHex("02");
    g2.items.append(i2);
    groups.append(g1);
    groups.append(g2);

    SequenceRunner runner;
    runner.setGroups(groups);
    QVERIFY(runner.start());

    QByteArray payload;
    runner.currentPayload(&payload, nullptr);
    QCOMPARE(payload, QByteArray::fromHex("01"));
    runner.advance();
    runner.currentPayload(&payload, nullptr);
    QCOMPARE(payload, QByteArray::fromHex("02"));

    // 全部发完后应回到第一组
    runner.advance();
    QCOMPARE(runner.currentGroupIndex(), 0);
    QCOMPARE(runner.currentItemIndex(), 0);
    QVERIFY(runner.currentPayload(&payload, nullptr));
    QCOMPARE(payload, QByteArray::fromHex("01"));
    QVERIFY(runner.isRunning());
}

void TestCore::sequenceRunnerSkipsEmptyGroups()
{
    QVector<SequenceGroup> groups;
    SequenceGroup empty;
    empty.name = QStringLiteral("空组");
    groups.append(empty);

    SequenceGroup g2;
    SequenceItem i;
    i.payload = QByteArray::fromHex("AA");
    g2.items.append(i);
    groups.append(g2);

    SequenceRunner runner;
    runner.setGroups(groups);
    QVERIFY(runner.start());
    QCOMPARE(runner.currentGroupIndex(), 1);

    QByteArray payload;
    QVERIFY(runner.currentPayload(&payload, nullptr));
    QCOMPARE(payload, QByteArray::fromHex("AA"));
}

void TestCore::sequenceRunnerStartFailsWhenEmpty()
{
    SequenceRunner runner;
    QVERIFY(!runner.start());
    QVERIFY(!runner.isRunning());
    QVERIFY(!runner.currentPayload(nullptr, nullptr));
}

QTEST_GUILESS_MAIN(TestCore)
#include "test_core.moc"