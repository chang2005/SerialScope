#include "utils/SettingsManager.h"

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>

QSettings *SettingsManager::settings()
{
    if (!QCoreApplication::organizationName().isEmpty()) {
        static QSettings instance;
        return &instance;
    }
    // 未设置应用信息时（例如单元测试）退回显式组织名
    static QSettings instance(QStringLiteral("SerialScope"), QStringLiteral("SerialScope"));
    return &instance;
}

void SettingsManager::saveSerialConfig(const SerialConfig &config)
{
    QSettings *s = settings();
    s->beginGroup(QStringLiteral("serial"));
    s->setValue(QStringLiteral("portName"), config.portName);
    s->setValue(QStringLiteral("baudRate"), config.baudRate);
    s->setValue(QStringLiteral("dataBits"), static_cast<int>(config.dataBits));
    s->setValue(QStringLiteral("parity"), static_cast<int>(config.parity));
    s->setValue(QStringLiteral("stopBits"), static_cast<int>(config.stopBits));
    s->setValue(QStringLiteral("flowControl"), static_cast<int>(config.flowControl));
    s->endGroup();
}

SerialConfig SettingsManager::loadSerialConfig()
{
    QSettings *s = settings();
    SerialConfig config;
    s->beginGroup(QStringLiteral("serial"));
    config.portName = s->value(QStringLiteral("portName")).toString();
    config.baudRate = s->value(QStringLiteral("baudRate"), 115200).toInt();
    config.dataBits = static_cast<QSerialPort::DataBits>(
        s->value(QStringLiteral("dataBits"), static_cast<int>(QSerialPort::Data8)).toInt());
    config.parity = static_cast<QSerialPort::Parity>(
        s->value(QStringLiteral("parity"), static_cast<int>(QSerialPort::NoParity)).toInt());
    config.stopBits = static_cast<QSerialPort::StopBits>(
        s->value(QStringLiteral("stopBits"), static_cast<int>(QSerialPort::OneStop)).toInt());
    config.flowControl = static_cast<QSerialPort::FlowControl>(
        s->value(QStringLiteral("flowControl"), static_cast<int>(QSerialPort::NoFlowControl)).toInt());
    s->endGroup();
    return config;
}

QString SettingsManager::lastDirectory()
{
    const QString fallback = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return settings()->value(QStringLiteral("general/lastDir"), fallback).toString();
}

void SettingsManager::setLastDirectory(const QString &path)
{
    if (!path.isEmpty())
        settings()->setValue(QStringLiteral("general/lastDir"), path);
}

void SettingsManager::save()
{
    settings()->sync();
}
