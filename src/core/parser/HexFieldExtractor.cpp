#include "core/parser/HexFieldExtractor.h"

#include <QtEndian>

#include <cstring>
#include <limits>

namespace {
double nanValue()
{
    return std::numeric_limits<double>::quiet_NaN();
}
} // namespace

int HexFieldExtractor::fieldSize(FieldType type)
{
    switch (type) {
    case FieldType::Int8:
    case FieldType::UInt8:
        return 1;
    case FieldType::Int16:
    case FieldType::UInt16:
        return 2;
    case FieldType::Int32:
    case FieldType::UInt32:
    case FieldType::Float32:
        return 4;
    }
    return 0;
}

QString HexFieldExtractor::fieldTypeName(FieldType type)
{
    switch (type) {
    case FieldType::Int8:    return QStringLiteral("int8");
    case FieldType::UInt8:   return QStringLiteral("uint8");
    case FieldType::Int16:   return QStringLiteral("int16");
    case FieldType::UInt16:  return QStringLiteral("uint16");
    case FieldType::Int32:   return QStringLiteral("int32");
    case FieldType::UInt32:  return QStringLiteral("uint32");
    case FieldType::Float32: return QStringLiteral("float32");
    }
    return QStringLiteral("uint16");
}

FieldType HexFieldExtractor::fieldTypeFromName(const QString &name, bool *ok)
{
    const QString n = name.trimmed().toLower();
    if (ok)
        *ok = true;
    if (n == QLatin1String("int8"))    return FieldType::Int8;
    if (n == QLatin1String("uint8"))   return FieldType::UInt8;
    if (n == QLatin1String("int16"))   return FieldType::Int16;
    if (n == QLatin1String("uint16"))  return FieldType::UInt16;
    if (n == QLatin1String("int32"))   return FieldType::Int32;
    if (n == QLatin1String("uint32"))  return FieldType::UInt32;
    if (n == QLatin1String("float32")) return FieldType::Float32;
    if (ok)
        *ok = false;
    return FieldType::UInt16;
}

QVector<double> HexFieldExtractor::extract(const QByteArray &frame, const HexParseSpec &spec,
                                           bool *ok)
{
    QVector<double> values;
    values.reserve(spec.fields.size());
    bool allOk = true;

    for (const HexField &field : spec.fields) {
        const int size = fieldSize(field.type);

        if (size <= 0 || field.offset < 0 || field.offset > frame.size()
            || field.offset + size > frame.size()) {
            values.append(nanValue());
            allOk = false;
            continue;
        }

        const uchar *base = reinterpret_cast<const uchar *>(frame.constData()) + field.offset;
        double value = nanValue();
        switch (field.type) {
        case FieldType::Int8:
            value = static_cast<double>(static_cast<qint8>(base[0]));
            break;
        case FieldType::UInt8:
            value = static_cast<double>(base[0]);
            break;
        case FieldType::Int16: {
            const quint16 raw = spec.bigEndian ? qFromBigEndian<quint16>(base)
                                               : qFromLittleEndian<quint16>(base);
            value = static_cast<double>(static_cast<qint16>(raw));
            break;
        }
        case FieldType::UInt16: {
            const quint16 raw = spec.bigEndian ? qFromBigEndian<quint16>(base)
                                               : qFromLittleEndian<quint16>(base);
            value = static_cast<double>(raw);
            break;
        }
        case FieldType::Int32: {
            const quint32 raw = spec.bigEndian ? qFromBigEndian<quint32>(base)
                                               : qFromLittleEndian<quint32>(base);
            value = static_cast<double>(static_cast<qint32>(raw));
            break;
        }
        case FieldType::UInt32: {
            const quint32 raw = spec.bigEndian ? qFromBigEndian<quint32>(base)
                                               : qFromLittleEndian<quint32>(base);
            value = static_cast<double>(raw);
            break;
        }
        case FieldType::Float32: {
            const quint32 raw = spec.bigEndian ? qFromBigEndian<quint32>(base)
                                               : qFromLittleEndian<quint32>(base);
            float f = 0.0f;
            std::memcpy(&f, &raw, sizeof(float));
            value = static_cast<double>(f);
            break;
        }
        }

        values.append(value);
    }

    if (ok)
        *ok = allOk;
    return values;
}
