#include "ui/WaveformPanel.h"

#include "core/parser/HexCodec.h"
#include "ui/PlotWidget.h"
#include "ui/Theme.h"
#include "ui/dialogs/HexFieldConfigDialog.h"
#include "utils/SettingsManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLayoutItem>
#include <QMessageBox>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {

QString csvField(const QString &text)
{
    if (!text.contains(QLatin1Char(',')) && !text.contains(QLatin1Char('"'))
        && !text.contains(QLatin1Char('\n'))) {
        return text;
    }
    QString escaped = text;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

} // namespace

WaveformPanel::WaveformPanel(QWidget *parent)
    : QWidget(parent)
    , m_statusTimer(new QTimer(this))
{
    // 默认可用的 HEX 协议：AA 55 | len | cmd | CH1(u16) | CH2(u16) | CRC16
    m_hexSpec.frame.header = QByteArray::fromHex("AA55");
    m_hexSpec.frame.lengthMode = FrameSpec::LengthMode::LengthByte;
    m_hexSpec.frame.lengthByteOffset = 2;
    m_hexSpec.frame.lengthAdjust = 6;
    m_hexSpec.frame.fixedFrameLength = 16;
    m_hexSpec.frame.verifyCrc16Modbus = false;
    m_hexSpec.frame.maxFrameLength = 4096;
    m_hexSpec.bigEndian = false;
    m_hexSpec.fields = {HexField{QStringLiteral("CH1"), 3, FieldType::UInt16},
                        HexField{QStringLiteral("CH2"), 5, FieldType::UInt16}};

    setupUi();

    m_asciiParser.onValues = [this](const QVector<double> &values) {
        appendAsciiValues(values);
    };
    m_asciiParser.onOverflow = [this] { ++m_overflowCount; };
    m_frameParser.onFrame = [this](const QByteArray &frame) { appendHexFrame(frame); };
    m_frameParser.onBytesDiscarded = [this](int bytes) { m_skippedBytes += bytes; };

    restoreSettings();

    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this,
            &WaveformPanel::onSourceChanged);
    connect(m_windowCombo, &QComboBox::currentIndexChanged, this,
            &WaveformPanel::onWindowChanged);
    connect(m_pauseButton, &QToolButton::toggled, this, &WaveformPanel::onPauseToggled);
    connect(m_statusTimer, &QTimer::timeout, this, [this] {
        updateStatus();
        updateLegendValues();
    });

    m_clock.start();
    resetPipeline(true);
    m_statusTimer->start(300);
}

void WaveformPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(8);

    auto *sourceLabel = new QLabel(tr("数据源"), this);
    sourceLabel->setProperty("muted", true);
    toolbar->addWidget(sourceLabel);

    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->addItem(tr("ASCII 行解析"), 0);
    m_sourceCombo->addItem(tr("HEX 帧解析"), 1);
    m_sourceCombo->setFixedWidth(132);
    toolbar->addWidget(m_sourceCombo);

    m_configButton = new QToolButton(this);
    m_configButton->setText(tr("协议配置…"));
    m_configButton->setToolTip(tr("配置帧头、长度域、CRC 与数据通道字段"));
    connect(m_configButton, &QToolButton::clicked, this, &WaveformPanel::onConfigClicked);
    toolbar->addWidget(m_configButton);

    toolbar->addStretch(1);

    m_pauseButton = new QToolButton(this);
    m_pauseButton->setText(tr("暂停"));
    m_pauseButton->setCheckable(true);
    m_pauseButton->setToolTip(tr("只冻结曲线显示，数据仍在采集，恢复后立即显示"));
    toolbar->addWidget(m_pauseButton);

    auto *clearButton = new QToolButton(this);
    clearButton->setText(tr("清空"));
    clearButton->setToolTip(tr("清空波形数据并重置解析器与时间基准"));
    connect(clearButton, &QToolButton::clicked, this, &WaveformPanel::onClearClicked);
    toolbar->addWidget(clearButton);

    layout->addLayout(toolbar);

    m_legendHost = new QWidget(this);
    m_legendLayout = new QGridLayout(m_legendHost);
    m_legendLayout->setContentsMargins(4, 0, 4, 0);
    m_legendLayout->setHorizontalSpacing(14);
    m_legendLayout->setVerticalSpacing(4);
    layout->addWidget(m_legendHost);

    m_plot = new PlotWidget(this);
    m_plot->setWindowSeconds(10.0);
    layout->addWidget(m_plot, 1);

    auto *footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(8);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setProperty("muted", true);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    footer->addWidget(m_statusLabel, 1);

    auto *windowLabel = new QLabel(tr("窗口"), this);
    windowLabel->setProperty("muted", true);
    footer->addWidget(windowLabel);

    m_windowCombo = new QComboBox(this);
    m_windowCombo->addItem(tr("5 秒"), 5);
    m_windowCombo->addItem(tr("10 秒"), 10);
    m_windowCombo->addItem(tr("30 秒"), 30);
    m_windowCombo->addItem(tr("60 秒"), 60);
    m_windowCombo->setFixedWidth(76);
    footer->addWidget(m_windowCombo);

    auto *exportButton = new QToolButton(this);
    exportButton->setText(tr("导出 CSV"));
    connect(exportButton, &QToolButton::clicked, this, &WaveformPanel::exportCsv);
    footer->addWidget(exportButton);

    layout->addLayout(footer);
}

void WaveformPanel::restoreSettings()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("waveform"));
    const int source = s->value(QStringLiteral("source"), 0).toInt();
    const int window = s->value(QStringLiteral("windowSeconds"), 10).toInt();
    const QString specJson = s->value(QStringLiteral("hexSpec")).toString();
    s->endGroup();

    const int sourceIndex = m_sourceCombo->findData(source);
    m_sourceCombo->setCurrentIndex(sourceIndex >= 0 ? sourceIndex : 0);
    m_source = m_sourceCombo->currentData().toInt() == 1 ? Source::Hex : Source::Ascii;

    int windowIndex = m_windowCombo->findData(window);
    if (windowIndex < 0)
        windowIndex = m_windowCombo->findData(10);
    m_windowCombo->setCurrentIndex(windowIndex >= 0 ? windowIndex : 1);

    if (!specJson.isEmpty())
        loadSpecFromJson(specJson);

    m_plot->setWindowSeconds(m_windowCombo->currentData().toInt());
    m_configButton->setEnabled(m_source == Source::Hex);
}

void WaveformPanel::saveSettings()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("waveform"));
    s->setValue(QStringLiteral("source"), m_source == Source::Hex ? 1 : 0);
    s->setValue(QStringLiteral("windowSeconds"), m_windowCombo->currentData().toInt());
    s->setValue(QStringLiteral("hexSpec"), specToJson());
    s->endGroup();
}

QString WaveformPanel::specToJson() const
{
    QJsonObject root;
    root[QStringLiteral("header")] = QString::fromLatin1(m_hexSpec.frame.header.toHex());
    root[QStringLiteral("lengthMode")] = static_cast<int>(m_hexSpec.frame.lengthMode);
    root[QStringLiteral("lengthByteOffset")] = m_hexSpec.frame.lengthByteOffset;
    root[QStringLiteral("lengthAdjust")] = m_hexSpec.frame.lengthAdjust;
    root[QStringLiteral("fixedFrameLength")] = m_hexSpec.frame.fixedFrameLength;
    root[QStringLiteral("verifyCrc16Modbus")] = m_hexSpec.frame.verifyCrc16Modbus;
    root[QStringLiteral("maxFrameLength")] = m_hexSpec.frame.maxFrameLength;
    root[QStringLiteral("bigEndian")] = m_hexSpec.bigEndian;

    QJsonArray fields;
    for (const HexField &field : m_hexSpec.fields) {
        QJsonObject object;
        object[QStringLiteral("name")] = field.name;
        object[QStringLiteral("offset")] = field.offset;
        object[QStringLiteral("type")] = HexFieldExtractor::fieldTypeName(field.type);
        fields.append(object);
    }
    root[QStringLiteral("fields")] = fields;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void WaveformPanel::loadSpecFromJson(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject())
        return;
    const QJsonObject root = document.object();

    HexParseSpec spec = m_hexSpec;
    QString headerError;
    const QByteArray header = HexCodec::parseHex(
        root.value(QStringLiteral("header")).toString(), &headerError);
    if (headerError.isEmpty() && !header.isEmpty())
        spec.frame.header = header;

    if (root.contains(QStringLiteral("lengthMode"))) {
        const int mode = root.value(QStringLiteral("lengthMode")).toInt();
        if (mode == static_cast<int>(FrameSpec::LengthMode::LengthByte)
            || mode == static_cast<int>(FrameSpec::LengthMode::Fixed)) {
            spec.frame.lengthMode = static_cast<FrameSpec::LengthMode>(mode);
        }
    }
    spec.frame.lengthByteOffset =
        root.value(QStringLiteral("lengthByteOffset")).toInt(spec.frame.lengthByteOffset);
    spec.frame.lengthAdjust =
        root.value(QStringLiteral("lengthAdjust")).toInt(spec.frame.lengthAdjust);
    spec.frame.fixedFrameLength =
        root.value(QStringLiteral("fixedFrameLength")).toInt(spec.frame.fixedFrameLength);
    spec.frame.verifyCrc16Modbus =
        root.value(QStringLiteral("verifyCrc16Modbus")).toBool(false);
    spec.frame.maxFrameLength =
        root.value(QStringLiteral("maxFrameLength")).toInt(4096);
    spec.bigEndian = root.value(QStringLiteral("bigEndian")).toBool(false);

    QVector<HexField> fields;
    const QJsonArray fieldArray = root.value(QStringLiteral("fields")).toArray();
    for (const QJsonValue &value : fieldArray) {
        const QJsonObject object = value.toObject();
        HexField field;
        field.name = object.value(QStringLiteral("name")).toString().trimmed();
        if (field.name.isEmpty())
            field.name = tr("CH%1").arg(fields.size() + 1);
        field.offset = object.value(QStringLiteral("offset")).toInt(0);
        bool ok = false;
        field.type = HexFieldExtractor::fieldTypeFromName(
            object.value(QStringLiteral("type")).toString(), &ok);
        if (!ok)
            field.type = FieldType::UInt16;
        fields.append(field);
    }
    if (!fields.isEmpty())
        spec.fields = fields;

    // 基本合法性保护
    spec.frame.maxFrameLength = qBound(8, spec.frame.maxFrameLength, 1024 * 1024);
    spec.frame.lengthByteOffset = qMax(0, spec.frame.lengthByteOffset);
    spec.frame.fixedFrameLength = qBound(1, spec.frame.fixedFrameLength,
                                         spec.frame.maxFrameLength);
    m_hexSpec = spec;
}

void WaveformPanel::resetPipeline(bool resetClock)
{
    m_asciiParser.reset();
    m_frameParser.reset();
    m_frameParser.setSpec(m_hexSpec.frame);

    m_plot->removeAllSeries();
    m_plot->clearData();
    rebuildLegend();

    m_rxBytes = 0;
    m_lineCount = 0;
    m_frameCount = 0;
    m_skippedBytes = 0;
    m_overflowCount = 0;
    m_fieldErrorCount = 0;

    if (resetClock)
        m_clock.restart();

    if (m_source == Source::Hex)
        rebuildHexSeries();

    m_plot->setPaused(m_paused);
    updateStatus();
}

void WaveformPanel::rebuildHexSeries()
{
    m_plot->removeAllSeries();
    for (int i = 0; i < m_hexSpec.fields.size(); ++i) {
        const HexField &field = m_hexSpec.fields.at(i);
        m_plot->addSeries(field.name.isEmpty() ? tr("CH%1").arg(i + 1) : field.name,
                          Theme::seriesColor(i));
    }
    rebuildLegend();
}

void WaveformPanel::ensureAsciiSeries(int count)
{
    const int target = qMin(count, 8);
    bool added = false;
    while (m_plot->seriesCount() < target) {
        const int index = m_plot->seriesCount();
        m_plot->addSeries(tr("CH%1").arg(index + 1), Theme::seriesColor(index));
        added = true;
    }
    if (added)
        rebuildLegend();
}

void WaveformPanel::appendAsciiValues(const QVector<double> &values)
{
    if (values.isEmpty())
        return;
    ensureAsciiSeries(values.size());

    const double x = m_clock.isValid() ? m_clock.elapsed() / 1000.0 : 0.0;
    const int count = qMin(values.size(), m_plot->seriesCount());
    for (int i = 0; i < count; ++i)
        m_plot->appendPoint(i, x, values.at(i));
    ++m_lineCount;
}

void WaveformPanel::appendHexFrame(const QByteArray &frame)
{
    bool ok = false;
    const QVector<double> values = HexFieldExtractor::extract(frame, m_hexSpec, &ok);
    if (!ok)
        ++m_fieldErrorCount;

    const double x = m_clock.isValid() ? m_clock.elapsed() / 1000.0 : 0.0;
    for (int i = 0; i < values.size() && i < m_plot->seriesCount(); ++i) {
        if (std::isfinite(values.at(i)))
            m_plot->appendPoint(i, x, values.at(i));
    }
    ++m_frameCount;
}

void WaveformPanel::feed(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    m_rxBytes += data.size();
    if (!m_clock.isValid())
        m_clock.start();

    if (m_source == Source::Ascii)
        m_asciiParser.feed(data);
    else
        m_frameParser.feed(data);
}

void WaveformPanel::clear()
{
    resetPipeline(true);
}

void WaveformPanel::setPaused(bool paused)
{
    m_pauseButton->setChecked(paused);
}

void WaveformPanel::onPauseToggled(bool paused)
{
    m_paused = paused;
    m_pauseButton->setText(paused ? tr("继续") : tr("暂停"));
    m_plot->setPaused(paused);
    updateStatus();
}

void WaveformPanel::onClearClicked()
{
    if (m_lineCount + m_frameCount > 0) {
        const auto answer = QMessageBox::question(
            this, tr("清空波形"),
            tr("确定清空波形数据和解析状态吗？此操作不可恢复。"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    resetPipeline(true);
    emit statusMessage(tr("波形已清空"));
}

void WaveformPanel::onSourceChanged()
{
    m_source = m_sourceCombo->currentData().toInt() == 1 ? Source::Hex : Source::Ascii;
    m_configButton->setEnabled(m_source == Source::Hex);
    saveSettings();
    resetPipeline(true);
    emit statusMessage(m_source == Source::Hex ? tr("波形数据源：HEX 帧解析")
                                               : tr("波形数据源：ASCII 行解析"));
}

void WaveformPanel::onWindowChanged()
{
    m_plot->setWindowSeconds(m_windowCombo->currentData().toInt());
    saveSettings();
}

void WaveformPanel::onConfigClicked()
{
    HexFieldConfigDialog dialog(this);
    dialog.setSpec(m_hexSpec);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_hexSpec = dialog.spec();
    saveSettings();
    if (m_source == Source::Hex)
        resetPipeline(true);
    emit statusMessage(tr("HEX 协议配置已更新"));
}

void WaveformPanel::rebuildLegend()
{
    m_legendChecks.clear();
    m_legendValues.clear();
    while (QLayoutItem *item = m_legendLayout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }

    for (int i = 0; i < m_plot->seriesCount(); ++i) {
        auto *container = new QWidget(m_legendHost);
        auto *row = new QHBoxLayout(container);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(5);

        auto *swatch = new QFrame(container);
        swatch->setFixedSize(10, 10);
        swatch->setStyleSheet(QStringLiteral("background:%1; border-radius:2px;")
                                  .arg(m_plot->seriesColor(i).name()));
        row->addWidget(swatch);

        auto *check = new QCheckBox(m_plot->seriesName(i), container);
        check->setChecked(m_plot->seriesVisible(i));
        connect(check, &QCheckBox::toggled, this, [this, i](bool on) {
            m_plot->setSeriesVisible(i, on);
        });
        row->addWidget(check);

        auto *value = new QLabel(QStringLiteral("—"), container);
        value->setProperty("muted", true);
        value->setMinimumWidth(52);
        row->addWidget(value);

        m_legendLayout->addWidget(container, i / 4, i % 4);
        m_legendChecks.append(check);
        m_legendValues.append(value);
    }
}

void WaveformPanel::updateLegendValues()
{
    for (int i = 0; i < m_legendValues.size() && i < m_plot->seriesCount(); ++i) {
        const QPointF last = m_plot->lastPoint(i);
        if (std::isfinite(last.y()))
            m_legendValues.at(i)->setText(QString::number(last.y(), 'g', 6));
        else
            m_legendValues.at(i)->setText(QStringLiteral("—"));
    }
}

void WaveformPanel::updateStatus()
{
    bool hasPoints = false;
    for (int i = 0; i < m_plot->seriesCount(); ++i) {
        if (!m_plot->points(i).isEmpty()) {
            hasPoints = true;
            break;
        }
    }

    QString text;
    if (m_source == Source::Ascii) {
        text = tr("已接收 %1 · 行 %2 · 通道 %3")
                   .arg(formatBytes(m_rxBytes))
                   .arg(m_lineCount)
                   .arg(m_plot->seriesCount());
        if (m_overflowCount > 0)
            text += tr(" · 超长行丢弃 %1 次").arg(m_overflowCount);
    } else {
        text = tr("已接收 %1 · 帧 %2 · 跳过 %3 · 通道 %4")
                   .arg(formatBytes(m_rxBytes))
                   .arg(m_frameCount)
                   .arg(formatBytes(m_skippedBytes))
                   .arg(m_plot->seriesCount());
        if (m_fieldErrorCount > 0)
            text += tr(" · 字段越界 %1 帧").arg(m_fieldErrorCount);
    }

    if (m_rxBytes > 0 && !hasPoints)
        text += tr(" · 尚未解析出数值，请检查数据格式或协议配置");
    else if (m_rxBytes == 0)
        text += tr(" · 等待数据");

    if (m_paused)
        text += tr(" · 已暂停，数据仍在采集");

    m_statusLabel->setText(text);
}

QString WaveformPanel::formatBytes(qint64 bytes) const
{
    if (bytes < 1024)
        return tr("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return tr("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    return tr("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 2));
}

void WaveformPanel::exportCsv()
{
    bool hasPoints = false;
    for (int i = 0; i < m_plot->seriesCount(); ++i) {
        if (!m_plot->points(i).isEmpty()) {
            hasPoints = true;
            break;
        }
    }
    if (!hasPoints) {
        QMessageBox::information(this, tr("导出 CSV"), tr("当前没有可导出的波形数据。"));
        return;
    }

    const QString dir = SettingsManager::lastDirectory();
    const QString defaultName = QStringLiteral("waveform_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("导出波形 CSV"), dir + QLatin1Char('/') + defaultName,
        tr("CSV 文件 (*.csv);;所有文件 (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出 CSV"),
                             tr("无法写入文件：%1").arg(file.errorString()));
        return;
    }

    file.write("\xEF\xBB\xBF");
    QTextStream out(&file);
    out << "# SerialScope waveform export\n";
    out << "# exported_at="
        << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    out << "# source=" << (m_source == Source::Hex ? "hex_frames" : "ascii_lines") << "\n";
    out << "# time_s is relative to this capture's t0\n";
    out << "time_s,channel,value\n";

    for (int i = 0; i < m_plot->seriesCount(); ++i) {
        const QString name = csvField(m_plot->seriesName(i));
        const QVector<QPointF> &points = m_plot->points(i);
        for (const QPointF &point : points) {
            out << QString::number(point.x(), 'f', 3) << ','
                << name << ','
                << QString::number(point.y(), 'g', 9) << '\n';
        }
    }
    out.flush();
    file.close();

    SettingsManager::setLastDirectory(QFileInfo(path).absolutePath());
    emit statusMessage(tr("波形已导出：%1").arg(path));
}
