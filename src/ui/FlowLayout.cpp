#include "ui/FlowLayout.h"

#include <QWidget>

FlowLayout::FlowLayout(QWidget *parent, int margin)
    : QLayout(parent)
{
    if (margin >= 0)
        setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin)
    : QLayout()
{
    if (margin >= 0)
        setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem *item = takeAt(0))
        delete item;
}

void FlowLayout::addItem(QLayoutItem *item)
{
    m_items.append(item);
}

int FlowLayout::horizontalSpacing() const
{
    return m_hSpace;
}

int FlowLayout::verticalSpacing() const
{
    return m_vSpace;
}

void FlowLayout::setHorizontalSpacing(int spacing)
{
    m_hSpace = spacing;
    invalidate();
}

void FlowLayout::setVerticalSpacing(int spacing)
{
    m_vSpace = spacing;
    invalidate();
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return Qt::Orientations();
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

int FlowLayout::count() const
{
    return m_items.size();
}

QLayoutItem *FlowLayout::itemAt(int index) const
{
    return m_items.value(index);
}

QLayoutItem *FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
    return m_items.takeAt(index);
}

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem *item : m_items)
        size = size.expandedTo(item->minimumSize());
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(),
                        margins.top() + margins.bottom());
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    const QMargins margins = contentsMargins();
    const QRect effective = rect.adjusted(margins.left(), margins.top(),
                                          -margins.right(), -margins.bottom());
    // 注意：heightForWidth 传入的高度为 0，这不是"无内容"，
    // 只看宽度是否还有效。
    if (effective.width() <= 0)
        return 0;

    int x = effective.x();
    int y = effective.y();
    int lineHeight = 0;
    int contentHeight = 0;

    for (QLayoutItem *item : m_items) {
        const QWidget *widget = item->widget();
        if (widget && widget->isHidden())
            continue;

        int spaceX = smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
        int spaceY = smartSpacing(QStyle::PM_LayoutVerticalSpacing);
        if (spaceX < 0)
            spaceX = m_hSpace;
        if (spaceY < 0)
            spaceY = m_vSpace;

        const QSize hint = item->sizeHint();
        int next = x + hint.width() + spaceX;
        if (next - spaceX > effective.right() && lineHeight > 0) {
            // 放不下，换行
            x = effective.x();
            y = y + lineHeight + spaceY;
            next = x + hint.width() + spaceX;
            lineHeight = 0;
        }

        if (!testOnly)
            item->setGeometry(QRect(QPoint(x, y), hint));

        x = next;
        lineHeight = qMax(lineHeight, hint.height());
        contentHeight = qMax(contentHeight, y + lineHeight - effective.y());
    }

    // 返回内容总高（含边距），供 heightForWidth 使用
    return contentHeight + margins.top() + margins.bottom();
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const
{
    QObject *parent = this->parent();
    if (!parent)
        return -1;

    if (parent->isWidgetType()) {
        QWidget *pw = static_cast<QWidget *>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    }
    return static_cast<QLayout *>(parent)->spacing();
}
