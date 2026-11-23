#pragma once

#include <QLayout>
#include <QList>
#include <QRect>
#include <QStyle>

/// 流式布局：控件按顺序排列，放不下时自动换到下一行。
///
/// 用于顶部参数栏这类控件数量多、窗口宽度不定的场景——比固定 QHBoxLayout
/// 更能适应窄窗口，不会把控件挤成不可用的宽度。
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent, int margin = -1);
    explicit FlowLayout(int margin = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem *item) override;
    int horizontalSpacing() const;
    int verticalSpacing() const;
    void setHorizontalSpacing(int spacing);
    void setVerticalSpacing(int spacing);

    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;
    QSize sizeHint() const override;

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    QList<QLayoutItem *> m_items;
    int m_hSpace = 6;
    int m_vSpace = 6;
};
