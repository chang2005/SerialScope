#pragma once

#include <QStyle>
#include <QWidget>

namespace WidgetUtil {

/// 重新应用控件样式：当控件的动态属性（如 error、muted）变化后调用，
/// 让 QSS 重新生效并触发重绘。
inline void repolish(QWidget *widget)
{
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace WidgetUtil
