#include "ui/ReceivePanel.h"

#include "ui/Theme.h"
#include "ui/WidgetUtil.h"
#include "utils/SettingsManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QToolButton>
#include <QVBoxLayout>

ReceivePanel::ReceivePanel(QWidget *parent)
    : QWidget(parent)
{
    m_decodeState = std::make_unique<QTextCodec::ConverterState>(
        QTextCodec::DefaultConversion);

    setupUi();
    restoreSettings();

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &ReceivePanel::onModeChanged);
    connect(m_encodingCombo, &QComboBox::currentIndexChanged, this,
            &ReceivePanel::onEncodingChanged);
    connect(m_pauseButton, &QToolButton::toggled, this, &ReceivePanel::onPauseToggled);
    connect(m_showTxCheck, &QCheckBox::toggled, this, [this](bool on) {
        saveSettings();
        emit showTxChanged(on);
    });
    connect(m_timestampCheck, &QCheckBox::toggled, this, [this] { saveSettings(); });
    connect(m_autoScrollCheck, &QCheckBox::toggled, this, [this] { saveSettings(); });
    connect(m_filterEdit, &QLineEdit::textChanged, this, &ReceivePanel::onFilterChanged);
    connect(m_filterRegexButton, &QToolButton::toggled, this, &ReceivePanel::onFilterModeToggled);
    connect(m_showControlCharsCheck, &QCheckBox::toggled, this,
            &ReceivePanel::onShowControlCharsToggled);
}

ReceivePanel::~ReceivePanel()
{
    saveSettings();
}

void ReceivePanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(8);

    auto *modeLabel = new QLabel(tr("显示"), this);
    modeLabel->setProperty("muted", true);
    toolbar->addWidget(modeLabel);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("文本"), false);
    m_modeCombo->addItem(tr("HEX"), true);
    m_modeCombo->setFixedWidth(78);
    toolbar->addWidget(m_modeCombo);

    m_encodingCombo = new QComboBox(this);
    m_encodingCombo->addItem(QStringLiteral("UTF-8"), static_cast<int>(TextEncoding::Utf8));
    m_encodingCombo->addItem(QStringLiteral("GBK"), static_cast<int>(TextEncoding::Gbk));
    m_encodingCombo->addItem(QStringLiteral("ASCII"), static_cast<int>(TextEncoding::Ascii));
    m_encodingCombo->setFixedWidth(84);
    m_encodingCombo->setToolTip(tr("文本模式下的字符编码"));
    toolbar->addWidget(m_encodingCombo);

    m_timestampCheck = new QCheckBox(tr("时间戳"), this);
    m_timestampCheck->setChecked(true);
    toolbar->addWidget(m_timestampCheck);

    m_autoScrollCheck = new QCheckBox(tr("自动滚动"), this);
    m_autoScrollCheck->setChecked(true);
    toolbar->addWidget(m_autoScrollCheck);

    m_showTxCheck = new QCheckBox(tr("显示发送"), this);
    m_showTxCheck->setChecked(true);
    toolbar->addWidget(m_showTxCheck);

    m_pauseButton = new QToolButton(this);
    m_pauseButton->setText(tr("暂停"));
    m_pauseButton->setCheckable(true);
    m_pauseButton->setToolTip(tr("暂停接收区刷新；数据仍会送入波形与日志，但暂停期间的内容不显示"));
    toolbar->addWidget(m_pauseButton);

    auto *clearButton = new QToolButton(this);
    clearButton->setText(tr("清空"));
    connect(clearButton, &QToolButton::clicked, this, &ReceivePanel::onClearClicked);
    toolbar->addWidget(clearButton);

    auto *saveButton = new QToolButton(this);
    saveButton->setText(tr("保存…"));
    saveButton->setToolTip(tr("把当前显示内容保存为文本文件"));
    connect(saveButton, &QToolButton::clicked, this, &ReceivePanel::onSaveClicked);
    toolbar->addWidget(saveButton);

    layout->addLayout(toolbar);

    // ---- 过滤与不可见字符：单独一行，避免把上面的工具栏挤扁 ----
    auto *filterRow = new QHBoxLayout();
    filterRow->setContentsMargins(0, 0, 0, 0);
    filterRow->setSpacing(8);

    auto *filterLabel = new QLabel(tr("过滤"), this);
    filterLabel->setProperty("muted", true);
    filterRow->addWidget(filterLabel);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("关键词或正则"));
    m_filterEdit->setClearButtonEnabled(true);
    m_filterEdit->setMinimumWidth(140);
    m_filterEdit->setToolTip(tr("只显示包含该关键词的行；命中片段会高亮"));
    filterRow->addWidget(m_filterEdit, 1);

    m_filterRegexButton = new QToolButton(this);
    m_filterRegexButton->setText(QStringLiteral(".*"));
    m_filterRegexButton->setCheckable(true);
    m_filterRegexButton->setToolTip(tr("按正则表达式过滤"));
    filterRow->addWidget(m_filterRegexButton);

    m_showControlCharsCheck = new QCheckBox(tr("控制字符"), this);
    m_showControlCharsCheck->setToolTip(
        tr("把 \\r \\n \\t 等不可见字符显示为 <CR> <LF> <TAB>"));
    filterRow->addWidget(m_showControlCharsCheck);

    layout->addLayout(filterRow);

    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setUndoRedoEnabled(false);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setMaximumBlockCount(20000);
    m_view->setFont(Theme::monospaceFont(10));
    m_view->setPlaceholderText(tr("接收到的数据将显示在这里"));
    layout->addWidget(m_view, 1);
}

void ReceivePanel::restoreSettings()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("receive"));
    m_hexMode = s->value(QStringLiteral("hexMode"), false).toBool();
    const int encoding = s->value(QStringLiteral("encoding"),
                                  static_cast<int>(TextEncoding::Utf8)).toInt();
    m_timestampCheck->setChecked(s->value(QStringLiteral("timestamps"), true).toBool());
    m_autoScrollCheck->setChecked(s->value(QStringLiteral("autoScroll"), true).toBool());
    m_showTxCheck->setChecked(s->value(QStringLiteral("showTx"), true).toBool());
    m_showControlCharsCheck->setChecked(
        s->value(QStringLiteral("showControlChars"), false).toBool());
    m_controlChars.setShowsAll(m_showControlCharsCheck->isChecked());
    const bool filterRegex = s->value(QStringLiteral("filterRegex"), false).toBool();
    const QString filterText = s->value(QStringLiteral("filterText")).toString();
    s->endGroup();

    m_modeCombo->blockSignals(true);
    m_modeCombo->setCurrentIndex(m_hexMode ? 1 : 0);
    m_modeCombo->blockSignals(false);

    m_encodingCombo->blockSignals(true);
    const int encodingIndex = m_encodingCombo->findData(encoding);
    m_encodingCombo->setCurrentIndex(encodingIndex >= 0 ? encodingIndex : 0);
    m_encodingCombo->blockSignals(false);

    m_encodingCombo->setEnabled(!m_hexMode);

    // 过滤器：先恢复正则开关，再恢复关键词，最后统一应用
    m_filterRegexButton->blockSignals(true);
    m_filterRegexButton->setChecked(filterRegex);
    m_filterRegexButton->blockSignals(false);
    m_filterEdit->blockSignals(true);
    m_filterEdit->setText(filterText);
    m_filterEdit->blockSignals(false);
    m_filter.setPattern(filterText, filterRegex);
}

void ReceivePanel::saveSettings()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("receive"));
    s->setValue(QStringLiteral("hexMode"), m_hexMode);
    s->setValue(QStringLiteral("encoding"), static_cast<int>(encoding()));
    s->setValue(QStringLiteral("timestamps"), m_timestampCheck->isChecked());
    s->setValue(QStringLiteral("autoScroll"), m_autoScrollCheck->isChecked());
    s->setValue(QStringLiteral("showTx"), m_showTxCheck->isChecked());
    s->setValue(QStringLiteral("showControlChars"), m_showControlCharsCheck->isChecked());
    s->setValue(QStringLiteral("filterText"), m_filterEdit->text());
    s->setValue(QStringLiteral("filterRegex"), m_filterRegexButton->isChecked());
    s->endGroup();
}

bool ReceivePanel::showTx() const
{
    return m_showTxCheck->isChecked();
}

void ReceivePanel::appendRx(const QByteArray &data)
{
    if (m_pauseButton->isChecked()) {
        m_pausedDroppedBytes += data.size();
        return;
    }
    appendData(data, false);
}

void ReceivePanel::appendTx(const QByteArray &data)
{
    if (!showTx())
        return;
    if (m_pauseButton->isChecked()) {
        m_pausedDroppedBytes += data.size();
        return;
    }
    appendData(data, true);
}

QString ReceivePanel::chunkPrefix(bool tx) const
{
    if (!m_timestampCheck->isChecked())
        return {};
    return QStringLiteral("[%1] %2 ")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
             tx ? QStringLiteral("TX") : QStringLiteral("RX"));
}

void ReceivePanel::appendData(const QByteArray &data, bool tx)
{
    if (data.isEmpty())
        return;

    const QString prefix = chunkPrefix(tx);
    QString text;

    if (m_hexMode) {
        constexpr int bytesPerLine = 16;
        const QString indent(prefix.size(), QLatin1Char(' '));
        bool first = true;
        for (int offset = 0; offset < data.size(); offset += bytesPerLine) {
            if (!first)
                text += prefix.isEmpty() ? QString() : indent;
            else if (!prefix.isEmpty())
                text += prefix;
            text += QString::fromLatin1(data.mid(offset, bytesPerLine).toHex(' ').toUpper());
            text += QLatin1Char('\n');
            first = false;
        }
    } else {
        const TextEncoding enc = encoding();
        QTextCodec::ConverterState *state =
            (enc == TextEncoding::Ascii) ? nullptr : m_decodeState.get();
        QString decoded = HexCodec::decode(data, enc, state);
        if (decoded.isEmpty())
            return;

        // 控制字符可视化：把 \r \n \t 显示为 <CR> <LF> <TAB>
        if (m_showControlCharsCheck->isChecked())
            decoded = m_controlChars.escape(decoded);

        if (!prefix.isEmpty()) {
            text = prefix + decoded;
            if (!text.endsWith(QLatin1Char('\n')) && !text.endsWith(QLatin1Char('\r')))
                text += QLatin1Char('\n');
        } else {
            text = decoded;
        }
    }

    if (text.isEmpty())
        return;

    // RX 不指定颜色，让文本跟随调色板，切换深浅主题时仍保持可读
    insertFilteredText(text, tx ? Theme::txColor() : QColor());
}

void ReceivePanel::insertText(const QString &text, const QColor &color)
{
    const int scrollValue = m_view->verticalScrollBar()->value();
    const bool autoScroll = m_autoScrollCheck->isChecked();

    QTextCursor cursor(m_view->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    if (color.isValid())
        format.setForeground(color);
    cursor.insertText(text, format);

    if (autoScroll)
        m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum());
    else
        m_view->verticalScrollBar()->setValue(scrollValue);
}

void ReceivePanel::insertFilteredText(const QString &text, const QColor &color)
{
    if (!m_filter.isActive()) {
        insertText(text, color);
        return;
    }

    // 逐行过滤：只保留含命中片段的行，并对命中片段着色。
    const QStringList lines = text.split(QLatin1Char('\n'));

    QTextCharFormat baseFormat;
    if (color.isValid())
        baseFormat.setForeground(color);

    QTextCharFormat hitFormat = baseFormat;
    hitFormat.setBackground(Theme::filterHighlightColor());
    hitFormat.setForeground(Theme::filterHighlightTextColor());

    const int scrollValue = m_view->verticalScrollBar()->value();
    const bool autoScroll = m_autoScrollCheck->isChecked();

    QTextCursor cursor(m_view->document());
    cursor.movePosition(QTextCursor::End);

    bool wroteAny = false;
    for (const QString &rawLine : lines) {
        QString line = rawLine;
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);

        const QList<QPair<int, int>> ranges = m_filter.matchRanges(line);
        if (ranges.isEmpty())
            continue;

        wroteAny = true;
        int pos = 0;
        for (const QPair<int, int> &range : ranges) {
            if (range.first > pos)
                cursor.insertText(line.mid(pos, range.first - pos), baseFormat);
            cursor.insertText(line.mid(range.first, range.second - range.first), hitFormat);
            pos = range.second;
        }
        if (pos < line.size())
            cursor.insertText(line.mid(pos), baseFormat);
        cursor.insertText(QStringLiteral("\n"), baseFormat);
    }

    if (!wroteAny)
        return;

    if (autoScroll)
        m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum());
    else
        m_view->verticalScrollBar()->setValue(scrollValue);
}

void ReceivePanel::appendSystemLine(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;

    QString line;
    if (!m_view->document()->isEmpty()) {
        QTextCursor endCursor(m_view->document());
        endCursor.movePosition(QTextCursor::End);
        if (!endCursor.atBlockStart())
            line += QLatin1Char('\n');
    }
    line += text;
    line += QLatin1Char('\n');
    insertText(line, Theme::secondaryTextColor());
}

void ReceivePanel::clear()
{
    m_view->clear();
    resetDecoder();
}

void ReceivePanel::setPaused(bool paused)
{
    m_pauseButton->setChecked(paused);
}

bool ReceivePanel::isPaused() const
{
    return m_pauseButton->isChecked();
}

void ReceivePanel::setHexMode(bool hex)
{
    m_modeCombo->setCurrentIndex(hex ? 1 : 0);
}

TextEncoding ReceivePanel::encoding() const
{
    return static_cast<TextEncoding>(m_encodingCombo->currentData().toInt());
}

void ReceivePanel::onModeChanged()
{
    m_hexMode = m_modeCombo->currentData().toBool();
    m_encodingCombo->setEnabled(!m_hexMode);
    resetDecoder();
    saveSettings();
    appendSystemLine(m_hexMode ? tr("── 显示模式：HEX ──") : tr("── 显示模式：文本 ──"));
}

void ReceivePanel::onEncodingChanged()
{
    resetDecoder();
    saveSettings();
}

void ReceivePanel::onPauseToggled(bool paused)
{
    m_pauseButton->setText(paused ? tr("继续") : tr("暂停"));
    if (paused) {
        resetDecoder();
        m_pausedDroppedBytes = 0;
    } else {
        if (m_pausedDroppedBytes > 0) {
            appendSystemLine(tr("── 暂停期间未显示 %1 字节（波形与日志未受影响） ──")
                                 .arg(m_pausedDroppedBytes));
            m_pausedDroppedBytes = 0;
        }
    }
}

void ReceivePanel::onClearClicked()
{
    clear();
}

void ReceivePanel::onFilterChanged()
{
    const bool ok = m_filter.setPattern(m_filterEdit->text(),
                                         m_filterRegexButton->isChecked());
    m_filterEdit->setProperty("error", !ok);
    WidgetUtil::repolish(m_filterEdit);
    if (!ok && m_filterEdit->isVisible()) {
        m_filterEdit->setToolTip(tr("正则表达式无效，已按纯文本匹配"));
    } else {
        m_filterEdit->setToolTip(tr("只显示包含该关键词的行；命中片段会高亮"));
    }
    saveSettings();
}

void ReceivePanel::onFilterModeToggled(bool regex)
{
    Q_UNUSED(regex);
    onFilterChanged();
}

void ReceivePanel::onShowControlCharsToggled(bool enabled)
{
    m_controlChars.setShowsAll(enabled);
    saveSettings();
}

void ReceivePanel::onSaveClicked()
{
    saveDisplay();
}

void ReceivePanel::saveDisplay()
{
    const QString dir = SettingsManager::lastDirectory();
    const QString defaultName = QStringLiteral("serial_display_%1.txt")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("保存显示内容"), dir + QLatin1Char('/') + defaultName,
        tr("文本文件 (*.txt);;所有文件 (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        appendSystemLine(tr("── 保存失败：%1 ──").arg(file.errorString()));
        return;
    }
    file.write(m_view->toPlainText().toUtf8());
    file.close();
    SettingsManager::setLastDirectory(QFileInfo(path).absolutePath());
    appendSystemLine(tr("── 显示内容已保存：%1 ──").arg(path));
}

void ReceivePanel::resetDecoder()
{
    m_decodeState = std::make_unique<QTextCodec::ConverterState>(
        QTextCodec::DefaultConversion);
}
