#pragma once

#include <QColor>
#include <QFont>
#include <QString>
#include <QVector>

class QApplication;

/// 全局主题：Fusion 风格 + 浅色/深色调色板与 QSS
class Theme
{
public:
    static void apply(QApplication *app, bool dark);
    static bool isDark();

    static const QVector<QColor> &seriesColors();
    static QColor seriesColor(int index) { return seriesColors().at(index % seriesColors().size()); }

    static QColor rxColor();
    static QColor txColor();
    static QColor secondaryTextColor();
    /// 过滤命中片段的高亮底色
    static QColor filterHighlightColor();
    /// 过滤命中片段的高亮文字色
    static QColor filterHighlightTextColor();

    static QFont monospaceFont(int pointSize = 10);

private:
    static QString styleSheet(bool dark);
    static bool s_dark;
};
