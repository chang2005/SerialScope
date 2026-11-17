// One-shot asset generator for SerialScope (not part of the application build).
// Renders the application icon at multiple sizes and prepares theme-aware
// GitHub marks from the official Octicon SVG.

#include <QGuiApplication>
#include <QBuffer>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QSvgRenderer>

static void drawAppIcon(QImage &image)
{
    const qreal s = image.width();
    image.fill(Qt::transparent);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF full(0, 0, s, s);
    QPainterPath outer;
    outer.addRoundedRect(full.adjusted(s * 0.015, s * 0.015, -s * 0.015, -s * 0.015),
                         s * 0.225, s * 0.225);

    QLinearGradient background(0, 0, s, s);
    background.setColorAt(0.00, QColor(5, 20, 38));
    background.setColorAt(0.55, QColor(9, 53, 92));
    background.setColorAt(1.00, QColor(7, 99, 124));
    p.fillPath(outer, background);

    p.save();
    p.setClipPath(outer);

    QRadialGradient glow(s * 0.55, s * 0.40, s * 0.65);
    glow.setColorAt(0.0, QColor(45, 212, 191, image.width() >= 32 ? 72 : 36));
    glow.setColorAt(1.0, QColor(45, 212, 191, 0));
    p.fillRect(full, glow);

    if (image.width() >= 24) {
        p.setPen(QPen(QColor(148, 233, 225, image.width() >= 64 ? 34 : 24),
                      qMax(1.0, s / 160.0)));
        for (int i = 1; i < 8; ++i) {
            const qreal x = s * i / 8.0;
            const qreal y = s * i / 8.0;
            p.drawLine(QPointF(x, s * 0.14), QPointF(x, s * 0.86));
            p.drawLine(QPointF(s * 0.10, y), QPointF(s * 0.90, y));
        }
    }

    QPainterPath wave;
    wave.moveTo(s * 0.145, s * 0.55);
    wave.cubicTo(s * 0.230, s * 0.55, s * 0.235, s * 0.245, s * 0.340, s * 0.245);
    wave.cubicTo(s * 0.445, s * 0.245, s * 0.440, s * 0.720, s * 0.545, s * 0.720);
    wave.cubicTo(s * 0.650, s * 0.720, s * 0.650, s * 0.365, s * 0.755, s * 0.365);
    wave.cubicTo(s * 0.830, s * 0.365, s * 0.850, s * 0.490, s * 0.900, s * 0.490);

    if (image.width() >= 32) {
        QPen glowPen(QColor(34, 211, 238, 55), s * 0.17,
                     Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(glowPen);
        p.drawPath(wave);
        glowPen.setColor(QColor(34, 211, 238, 95));
        glowPen.setWidthF(s * 0.115);
        p.setPen(glowPen);
        p.drawPath(wave);
    }

    QLinearGradient waveGradient(s * 0.12, 0, s * 0.90, 0);
    waveGradient.setColorAt(0.00, QColor(103, 232, 249));
    waveGradient.setColorAt(0.55, QColor(45, 212, 191));
    waveGradient.setColorAt(1.00, QColor(96, 165, 250));
    QPen wavePen(QBrush(waveGradient), qMax(2.0, s * 0.078),
                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(wavePen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(wave);

    if (image.width() >= 32) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(251, 191, 36));
        const qreal pinWidth = s * 0.075;
        const qreal pinHeight = s * 0.060;
        const qreal pinY[] = {0.245, 0.500, 0.755};
        for (const qreal y : pinY) {
            p.drawRoundedRect(QRectF(s * 0.055, s * y - pinHeight / 2.0,
                                     pinWidth, pinHeight),
                              pinHeight * 0.28, pinHeight * 0.28);
        }
    }

    if (image.width() >= 48) {
        QPen cursor(QColor(255, 255, 255, 110), qMax(1.0, s / 150.0), Qt::DashLine);
        cursor.setDashPattern({3, 3});
        p.setPen(cursor);
        p.drawLine(QPointF(s * 0.835, s * 0.14), QPointF(s * 0.835, s * 0.86));

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(45, 212, 191, 70));
        p.drawEllipse(QPointF(s * 0.835, s * 0.365), s * 0.050, s * 0.050);
        p.setBrush(QColor(255, 255, 255, 235));
        p.drawEllipse(QPointF(s * 0.835, s * 0.365), s * 0.024, s * 0.024);
    }

    p.restore();

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, image.width() >= 32 ? 70 : 45),
                  qMax(1.0, s / 110.0)));
    p.drawPath(outer);
}

static QImage colorized(const QImage &mask, const QColor &color)
{
    QImage out(mask.size(), QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawImage(0, 0, mask);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color);
    p.end();
    return out;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    if (argc != 3) {
        qWarning("usage: make_assets <output-icon-dir> <github-mark.svg>");
        return 2;
    }

    const QString outputDir = QString::fromLocal8Bit(argv[1]);
    const QString githubSvg = QString::fromLocal8Bit(argv[2]);
    if (!QDir().mkpath(outputDir)) {
        qWarning("cannot create %s", qPrintable(outputDir));
        return 1;
    }

    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    for (const int size : sizes) {
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        drawAppIcon(image);
        const QString path = outputDir + QStringLiteral("/app-%1.png").arg(size);
        if (!image.save(path, "PNG")) {
            qWarning("cannot write %s", qPrintable(path));
            return 1;
        }
    }

    QSvgRenderer renderer(githubSvg);
    if (!renderer.isValid()) {
        qWarning("cannot load GitHub mark SVG: %s", qPrintable(githubSvg));
        return 1;
    }

    QImage mark(128, 128, QImage::Format_ARGB32_Premultiplied);
    mark.fill(Qt::transparent);
    {
        QPainter p(&mark);
        renderer.render(&p, QRectF(0, 0, 128, 128));
    }
    if (!colorized(mark, QColor(36, 41, 47)).save(
            outputDir + QStringLiteral("/github-dark.png"), "PNG")) {
        qWarning("cannot write github-dark.png");
        return 1;
    }
    if (!colorized(mark, QColor(240, 246, 252)).save(
            outputDir + QStringLiteral("/github-light.png"), "PNG")) {
        qWarning("cannot write github-light.png");
        return 1;
    }

    return 0;
}
