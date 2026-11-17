#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

/// 实时滚动曲线控件（QPainter 自绘）。
/// 数据按像素列做 min/max 包络降采样，支持大量点平滑绘制。
class PlotWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlotWidget(QWidget *parent = nullptr);

    int addSeries(const QString &name, const QColor &color);
    void removeAllSeries();
    void clearData();

    void appendPoint(int index, double x, double y);

    int seriesCount() const { return m_series.size(); }
    QString seriesName(int index) const;
    QColor seriesColor(int index) const;
    bool seriesVisible(int index) const;
    void setSeriesVisible(int index, bool visible);
    void setSeriesName(int index, const QString &name);
    QPointF lastPoint(int index) const;
    const QVector<QPointF> &points(int index) const;

    void setWindowSeconds(double seconds);
    double windowSeconds() const { return m_windowSeconds; }

    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

    void setMaxPointsPerSeries(int maxPoints);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Series {
        QString name;
        QColor color;
        bool visible = true;
        QVector<QPointF> points;
    };

    void scheduleRepaint();
    double latestX() const { return m_latestX; }

    QVector<Series> m_series;
    double m_windowSeconds = 10.0;
    bool m_paused = false;
    int m_maxPointsPerSeries = 60000;
    double m_latestX = 0.0;
    double m_pausedX = 0.0;
    QTimer m_repaintTimer;
};
