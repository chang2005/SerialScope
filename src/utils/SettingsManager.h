#pragma once

#include "core/SerialConfig.h"

#include <QString>

class QSettings;

/// QSettings 封装：统一组织名/应用名，并提供常用配置的读写助手。
class SettingsManager
{
public:
    static QSettings *settings();

    static void saveSerialConfig(const SerialConfig &config);
    static SerialConfig loadSerialConfig();

    static QString lastDirectory();
    static void setLastDirectory(const QString &path);

    static void save();
};
