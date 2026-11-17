#include "ui/PlotWidget.h"

#include "ui/Theme.h"

#include <QFontMetricsF>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

double niceNumber(double value, bool roundUp)
{
    if (!(value > 0) || !std::isfinite(value))
        return 1.0;
    const double exponent = std::floor(std::log10(value));
    const double fraction = value / std::pow(10.0, exponent);
    double niceFraction;
    if (roundUp) {
        if (fraction <= 1.0)
            niceFraction = 1.0;
        else if (fraction <= 2.0)
            niceFraction = 2.0;
        else if (fraction <= 5.0)
            niceFraction = 5.0;
        else
            niceFraction = 10.0;
    } else {
        if (fraction < 1.5)
            niceFraction = 1.0;
        else if (fraction < 3.0)
            niceFraction = 2.0;
        else if (fraction < 7.0)
            niceFraction = 5.0;
        else
            niceFraction = 10.0;
    }
    return niceFraction * std::pow(10.0, exponent);
}

int decimalsForStep(double step)
{
    if (!(step > 0))
        return 1;
    const int decimals = static_cast<int>(std::ceil(-std::log10(step) - 1e-9));
    return std::clamp(decimals, 0, 6);
}

int lowerBoundIndex(const QVector<QPointF> &points, double x)
{
    return static_cast<int>(std::lower_bound(
        points.cbegin(), points.cend(), x,
        [](const QPointF &point, double value) { return point.x() < value; })
        - points.cbegin());
}

} // namespace

PlotWidget::PlotWidget(QWidget *parent)
    : QWidget(parent)
    , m_repaintTimer(this)
{
    setMinimumSize(280, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    m_repaintTimer.setSingleShot(true);
    m_repaintTimer.setInterval(33); // ~30 fps
    connect(&m_repaintTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
}

int PlotWidget::addSeries(const QString &name, const QColor &color)
{
    Series series;
    series.name = name;
    series.color = color;
    m_series.append(series);
    update();
    return m_series.size() - 1;
}

void PlotWidget::removeAllSeries()
{
    m_series.clear();
    m_latestX = 0.0;
    m_pausedX = 0.0;
    update();
}

void PlotWidget::clearData()
{
    for (Series &series : m_series)
        series.points.clear();
    m_latestX = 0.0;
    m_pausedX = 0.0;
    update();
}

void PlotWidget::appendPoint(int index, double x, double y)
{
    if (index < 0 || index >= m_series.size() || !std::isfinite(x) || !std::isfinite(y))
        return;

    Series &series = m_series[index];
    if (!series.points.isEmpty() && x < series.points.last().x())
        x = series.points.last().x();

    series.points.append(QPointF(x, y));

    if (series.points.size() > m_maxPointsPerSeries) {
        const int drop = qMax(1, m_maxPointsPerSeries / 4);
        series.points.remove(0, qMin(drop, series.points.size() - 1));
    }

    if (x > m_latestX)
        m_latestX = x;
    if (!m_paused)
        scheduleRepaint();
}

QString PlotWidget::seriesName(int index) const
{
    return (index >= 0 && index < m_series.size()) ? m_series.at(index).name : QString();
}

QColor PlotWidget::seriesColor(int index) const
{
    return (index >= 0 && index < m_series.size()) ? m_series.at(index).color : QColor();
}

bool PlotWidget::seriesVisible(int index) const
{
    return index >= 0 && index < m_series.size() && m_series.at(index).visible;
}

void PlotWidget::setSeriesVisible(int index, bool visible)
{
    if (index < 0 || index >= m_series.size())
        return;
    m_series[index].visible = visible;
    update();
}

void PlotWidget::setSeriesName(int index, const QString &name)
{
    if (index < 0 || index >= m_series.size())
        return;
    m_series[index].name = name;
    update();
}

QPointF PlotWidget::lastPoint(int index) const
{
    if (index < 0 || index >= m_series.size() || m_series.at(index).points.isEmpty())
        return QPointF(qQNaN(), qQNaN());
    return m_series.at(index).points.last();
}

const QVector<QPointF> &PlotWidget::points(int index) const
{
    static const QVector<QPointF> empty;
    return (index >= 0 && index < m_series.size()) ? m_series.at(index).points : empty;
}

void PlotWidget::setWindowSeconds(double seconds)
{
    m_windowSeconds = qMax(0.5, seconds);
    update();
}

void PlotWidget::setPaused(bool paused)
{
    if (paused && !m_paused)
        m_pausedX = m_latestX; // 冻结在暂停时刻；数据仍继续写入
    m_paused = paused;
    update();
}

void PlotWidget::setMaxPointsPerSeries(int maxPoints)
{
    m_maxPointsPerSeries = qMax(1000, maxPoints);
}

QSize PlotWidget::sizeHint() const
{
    return QSize(560, 340);
}

QSize PlotWidget::minimumSizeHint() const
{
    return QSize(280, 180);
}

void PlotWidget::scheduleRepaint()
{
    if (!m_repaintTimer.isActive())
        m_repaintTimer.start();
}

void PlotWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPalette pal = palette();
    const QColor base = pal.color(QPalette::Base);
    const QColor text = pal.color(QPalette::Text);
    QColor grid = text;
    grid.setAlpha(Theme::isDark() ? 34 : 26);
    QColor axis = text;
    axis.setAlpha(Theme::isDark() ? 100 : 80);

    painter.fillRect(rect(), base);

    const QRectF plotRect = QRectF(rect()).adjusted(58, 16, -18, -34);
    if (plotRect.width() < 60 || plotRect.height() < 40)
        return;

    bool hasData = false;
    for (const Series &series : m_series) {
        if (!series.points.isEmpty()) {
            hasData = true;
            break;
        }
    }

    double xMax = m_paused ? m_pausedX : m_latestX;
    if (!hasData || (m_paused && m_pausedX <= 0.0))
        xMax = m_windowSeconds;
    double xMin = qMax(0.0, xMax - m_windowSeconds);
    if (xMax - xMin < 1e-6)
        xMax = xMin + m_windowSeconds;

    // ---- Y 轴范围（只看可见窗口内的可见通道） ----
    double yMin = kInf;
    double yMax = -kInf;
    for (const Series &series : m_series) {
        if (!series.visible || series.points.isEmpty())
            continue;
        const int start = lowerBoundIndex(series.points, xMin);
        for (int i = start; i < series.points.size(); ++i) {
            const QPointF &point = series.points.at(i);
            if (point.x() > xMax)
                break;
            yMin = qMin(yMin, point.y());
            yMax = qMax(yMax, point.y());
        }
    }
    if (!std::isfinite(yMin) || !std::isfinite(yMax)) {
        yMin = -1.0;
        yMax = 1.0;
    }
    if (yMax - yMin < 1e-9) {
        const double padding = qMax(1.0, std::abs(yMax) * 0.1);
        yMin -= padding;
        yMax += padding;
    } else {
        const double padding = (yMax - yMin) * 0.12;
        yMin -= padding;
        yMax += padding;
    }

    const QFont baseFont = font();
    QFont smallFont = baseFont;
    smallFont.setPointSizeF(qMax(7.5, baseFont.pointSizeF() - 1.0));
    painter.setFont(smallFont);
    const QFontMetricsF metrics(smallFont);

    // 绘图区背景与边框
    QPainterPath plotPath;
    plotPath.addRoundedRect(plotRect, 6, 6);
    painter.fillPath(plotPath, base);
    painter.setPen(QPen(axis, 1));
    painter.drawPath(plotPath);

    painter.save();
    painter.setClipPath(plotPath);

    const double yScale = plotRect.height() / (yMax - yMin);
    auto yToPixel = [&](double value) {
        return plotRect.bottom() - (value - yMin) * yScale;
    };

    // 横向网格 + Y 轴刻度
    const double yStep = niceNumber((yMax - yMin) / 4.0, true);
    const int yDecimals = decimalsForStep(yStep);
    painter.setPen(QPen(grid, 1));
    for (double value = std::ceil(yMin / yStep) * yStep; value <= yMax + 1e-9;
         value += yStep) {
        const double y = yToPixel(value);
        painter.setPen(QPen(grid, 1));
        painter.drawLine(QPointF(plotRect.left(), y), QPointF(plotRect.right(), y));
        painter.setPen(QPen(text, 1));
        painter.drawText(QRectF(0, y - 9, plotRect.left() - 8, 18),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(value, 'f', yDecimals));
    }

    // 纵向网格 + X 轴刻度
    constexpr int xDivisions = 6;
    const int xDecimals = (xMax - xMin) >= 30.0 ? 0 : 1;
    for (int i = 0; i <= xDivisions; ++i) {
        const double ratio = static_cast<double>(i) / xDivisions;
        const double value = xMin + (xMax - xMin) * ratio;
        const double x = plotRect.left() + plotRect.width() * ratio;
        painter.setPen(QPen(grid, 1));
        painter.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
        painter.setPen(QPen(text, 1));
        painter.drawText(QRectF(x - 45, plotRect.bottom() + 6, 90, 18),
                         Qt::AlignHCenter | Qt::AlignTop,
                         QStringLiteral("%1s").arg(QString::number(value, 'f', xDecimals)));
    }

    // 曲线
    const double xScale = plotRect.width() / (xMax - xMin);
    for (const Series &series : m_series) {
        if (!series.visible || series.points.isEmpty())
            continue;

        const int start = lowerBoundIndex(series.points, xMin);
        int end = lowerBoundIndex(series.points, xMax);
        if (end < series.points.size() && series.points.at(end).x() <= xMax + 1e-12)
            ++end;
        const int count = end - start;
        if (count <= 0)
            continue;

        QPen pen(series.color, 1.6);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        if (count == 1) {
            const QPointF &point = series.points.at(start);
            painter.setBrush(series.color);
            painter.drawEllipse(QPointF(plotRect.left() + (point.x() - xMin) * xScale,
                                        yToPixel(point.y())),
                                2.6, 2.6);
            painter.setBrush(Qt::NoBrush);
            continue;
        }

        if (count <= static_cast<int>(plotRect.width()) * 2) {
            QPainterPath path;
            for (int i = start; i < end; ++i) {
                const QPointF &point = series.points.at(i);
                const QPointF mapped(plotRect.left() + (point.x() - xMin) * xScale,
                                     yToPixel(point.y()));
                if (i == start)
                    path.moveTo(mapped);
                else
                    path.lineTo(mapped);
            }
            painter.drawPath(path);
        } else {
            // 按像素列取 min/max 包络，绘制点数量与数据总量解耦
            const int columns = qMax(1, static_cast<int>(std::ceil(plotRect.width())));
            QVector<double> columnMin(columns, kInf);
            QVector<double> columnMax(columns, -kInf);
            QVector<bool> columnHas(columns, false);

            for (int i = start; i < end; ++i) {
                const QPointF &point = series.points.at(i);
                int col = static_cast<int>((point.x() - xMin) * xScale);
                col = std::clamp(col, 0, columns - 1);
                columnHas[col] = true;
                columnMin[col] = qMin(columnMin[col], point.y());
                columnMax[col] = qMax(columnMax[col], point.y());
            }

            QVector<QLineF> spikes;
            QPolygonF trend;
            spikes.reserve(columns);
            trend.reserve(columns);
            for (int col = 0; col < columns; ++col) {
                if (!columnHas.at(col))
                    continue;
                const double x = plotRect.left() + col + 0.5;
                const double top = yToPixel(columnMax.at(col));
                const double bottom = yToPixel(columnMin.at(col));
                if (bottom - top > 1.0)
                    spikes.append(QLineF(x, top, x, bottom));
                trend.append(QPointF(x, (top + bottom) * 0.5));
            }
            if (!spikes.isEmpty())
                painter.drawLines(spikes);
            if (trend.size() > 1)
                painter.drawPolyline(trend);
        }
    }

    painter.restore();

    // 空状态提示
    if (!hasData) {
        painter.setFont(baseFont);
        painter.setPen(Theme::secondaryTextColor());
        painter.drawText(plotRect, Qt::AlignCenter, tr("等待串口数据…"));
    }

    if (m_paused) {
        const QRectF badge(plotRect.left() + 10, plotRect.top() + 8, 62, 22);
        QColor badgeBg = Theme::secondaryTextColor();
        badgeBg.setAlpha(Theme::isDark() ? 60 : 40);
        painter.setPen(Qt::NoPen);
        painter.setBrush(badgeBg);
        painter.drawRoundedRect(badge, 5, 5);
        painter.setPen(Theme::secondaryTextColor());
        painter.setFont(smallFont);
        painter.drawText(badge, Qt::AlignCenter, tr("已暂停"));
    }
}
