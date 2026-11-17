#pragma once

#include "core/parser/AsciiValueParser.h"
#include "core/parser/DataFrameParser.h"
#include "core/parser/HexFieldExtractor.h"

#include <QElapsedTimer>
#include <QVector>
#include <QWidget>

class PlotWidget;
class QComboBox;
class QGridLayout;
class QLabel;
class QToolButton;
class QTimer;
class QCheckBox;

/// 波形面板：接收字节 → ASCII 行数值 / HEX 帧字段 → 多通道实时曲线
class WaveformPanel : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformPanel(QWidget *parent = nullptr);

    void feed(const QByteArray &data);
    void clear();
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }

signals:
    void statusMessage(const QString &message);

private slots:
    void onSourceChanged();
    void onPauseToggled(bool paused);
    void onClearClicked();
    void onWindowChanged();
    void onConfigClicked();
    void exportCsv();
    void updateStatus();
    void updateLegendValues();

private:
    enum class Source { Ascii, Hex };

    void setupUi();
    void restoreSettings();
    void saveSettings();
    void loadSpecFromJson(const QString &json);
    QString specToJson() const;

    void resetPipeline(bool resetClock);
    void rebuildHexSeries();
    void ensureAsciiSeries(int count);
    void appendAsciiValues(const QVector<double> &values);
    void appendHexFrame(const QByteArray &frame);
    void rebuildLegend();
    QString formatBytes(qint64 bytes) const;

    PlotWidget *m_plot = nullptr;
    QComboBox *m_sourceCombo = nullptr;
    QComboBox *m_windowCombo = nullptr;
    QToolButton *m_configButton = nullptr;
    QToolButton *m_pauseButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QGridLayout *m_legendLayout = nullptr;
    QWidget *m_legendHost = nullptr;
    QTimer *m_statusTimer = nullptr;

    QVector<QCheckBox *> m_legendChecks;
    QVector<QLabel *> m_legendValues;

    AsciiValueParser m_asciiParser;
    DataFrameParser m_frameParser;
    HexParseSpec m_hexSpec;
    QElapsedTimer m_clock;

    Source m_source = Source::Ascii;
    bool m_paused = false;
    qint64 m_rxBytes = 0;
    qint64 m_lineCount = 0;
    qint64 m_frameCount = 0;
    qint64 m_skippedBytes = 0;
    qint64 m_overflowCount = 0;
    qint64 m_fieldErrorCount = 0;
};
