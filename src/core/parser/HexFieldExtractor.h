#pragma once

#include "core/parser/DataFrameParser.h"

#include <QByteArray>
#include <QVector>

/// 二进制字段类型
enum class FieldType {
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Float32,
};

struct HexField {
    QString name;
    int offset = 0;
    FieldType type = FieldType::UInt16;
};

struct HexParseSpec {
    FrameSpec frame;
    QVector<HexField> fields;
    bool bigEndian = false;
};

/// 从完整帧中按字段表提取数值
class HexFieldExtractor
{
public:
    static int fieldSize(FieldType type);
    static QString fieldTypeName(FieldType type);
    static FieldType fieldTypeFromName(const QString &name, bool *ok = nullptr);

    /// 返回与 spec.fields 一一对应的数值；越界字段为 NaN。
    static QVector<double> extract(const QByteArray &frame, const HexParseSpec &spec,
                                   bool *ok = nullptr);
};
