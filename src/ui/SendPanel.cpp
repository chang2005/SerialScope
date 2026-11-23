#include "ui/SendPanel.h"

#include "core/parser/HexCodec.h"
#include "ui/Theme.h"
#include "ui/WidgetUtil.h"
#include "ui/dialogs/SequenceDialog.h"
#include "utils/CrcCalculator.h"
#include "utils/SettingsManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

void repolish(QWidget *widget)
{
    WidgetUtil::repolish(widget);
}

QLabel *makeMutedLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("muted", true);
    return label;
}

} // namespace

SendPanel::SendPanel(QWidget *parent)
    : QWidget(parent)
    , m_timedTimer(new QTimer(this))
    , m_sequenceTimer(new QTimer(this))
{
    setupUi();
    restoreSettings();

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &SendPanel::onModeChanged);
    connect(m_encodingCombo, &QComboBox::currentIndexChanged, this, [this] {
        saveSettings();
        updateControls();
    });
    connect(m_lineEndingCombo, &QComboBox::currentIndexChanged, this, [this] {
        saveSettings();
        updateControls();
    });
    connect(m_checksumCombo, &QComboBox::currentIndexChanged, this, [this] {
        saveSettings();
        updateControls();
    });
    connect(m_input, &QPlainTextEdit::textChanged, this, &SendPanel::onTextChanged);
    connect(m_sendButton, &QPushButton::clicked, this, &SendPanel::onSendClicked);
    connect(m_timedCheck, &QCheckBox::toggled, this, &SendPanel::onTimedToggled);
    connect(m_timedTimer, &QTimer::timeout, this, &SendPanel::onTimedTimeout);
    connect(m_intervalSpin, &QSpinBox::valueChanged, this, &SendPanel::onIntervalChanged);
    connect(m_countSpin, &QSpinBox::valueChanged, this, [this] { saveSettings(); });
    connect(m_historyCombo, &QComboBox::activated, this, &SendPanel::onHistoryActivated);
    connect(m_fileButton, &QPushButton::clicked, this, &SendPanel::onFileClicked);
    connect(m_convertButton, &QToolButton::clicked, this, &SendPanel::onConvertClicked);
    connect(m_quickSendSaveButton, &QToolButton::clicked, this, &SendPanel::onQuickSendSave);
    connect(m_quickSendDeleteButton, &QToolButton::clicked, this, &SendPanel::onQuickSendDelete);
    connect(m_sequenceEditButton, &QToolButton::clicked, this, &SendPanel::onSequenceEdit);
    connect(m_sequenceStartButton, &QToolButton::clicked, this, &SendPanel::onSequenceStart);
    connect(m_sequenceModeCombo, &QComboBox::currentIndexChanged, this,
            &SendPanel::onSequenceModeChanged);
    connect(m_sequenceTimer, &QTimer::timeout, this, &SendPanel::onTimedTimeout);

    auto *sendShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    sendShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(sendShortcut, &QShortcut::activated, this, &SendPanel::onSendClicked);

    m_hexMode = m_modeCombo->currentData().toBool();
    m_encodingCombo->setEnabled(!m_hexMode);
    m_lineEndingCombo->setEnabled(!m_hexMode);

    updateControls();
}

SendPanel::~SendPanel()
{
    saveSettings();
}

void SendPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *controls = new QHBoxLayout();
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(8);

    controls->addWidget(makeMutedLabel(tr("模式"), this));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("文本"), false);
    m_modeCombo->addItem(tr("HEX"), true);
    m_modeCombo->setFixedWidth(78);
    controls->addWidget(m_modeCombo);

    controls->addWidget(makeMutedLabel(tr("编码"), this));
    m_encodingCombo = new QComboBox(this);
    m_encodingCombo->addItem(QStringLiteral("UTF-8"), static_cast<int>(TextEncoding::Utf8));
    m_encodingCombo->addItem(QStringLiteral("GBK"), static_cast<int>(TextEncoding::Gbk));
    m_encodingCombo->addItem(QStringLiteral("ASCII"), static_cast<int>(TextEncoding::Ascii));
    m_encodingCombo->setFixedWidth(84);
    controls->addWidget(m_encodingCombo);

    controls->addWidget(makeMutedLabel(tr("行尾"), this));
    m_lineEndingCombo = new QComboBox(this);
    m_lineEndingCombo->addItem(tr("无"), 0);
    m_lineEndingCombo->addItem(QStringLiteral("CRLF"), 1);
    m_lineEndingCombo->addItem(QStringLiteral("LF"), 2);
    m_lineEndingCombo->addItem(QStringLiteral("CR"), 3);
    m_lineEndingCombo->setFixedWidth(78);
    controls->addWidget(m_lineEndingCombo);

    controls->addWidget(makeMutedLabel(tr("校验"), this));
    m_checksumCombo = new QComboBox(this);
    m_checksumCombo->addItem(tr("无"), static_cast<int>(CrcCalculator::AppendMode::None));
    m_checksumCombo->addItem(QStringLiteral("CRC16-Modbus"), static_cast<int>(CrcCalculator::AppendMode::Crc16Modbus));
    m_checksumCombo->addItem(QStringLiteral("CRC16-CCITT"), static_cast<int>(CrcCalculator::AppendMode::Crc16Ccitt));
    m_checksumCombo->addItem(QStringLiteral("Sum8"), static_cast<int>(CrcCalculator::AppendMode::Sum8));
    m_checksumCombo->addItem(QStringLiteral("Xor8"), static_cast<int>(CrcCalculator::AppendMode::Xor8));
    m_checksumCombo->addItem(QStringLiteral("LRC8"), static_cast<int>(CrcCalculator::AppendMode::Lrc8));
    m_checksumCombo->setFixedWidth(132);
    m_checksumCombo->setToolTip(tr("校验值附加在数据末尾、行尾之前；Modbus 低字节在前，CCITT 高字节在前"));
    controls->addWidget(m_checksumCombo);

    m_convertButton = new QToolButton(this);
    m_convertButton->setObjectName(QStringLiteral("convertButton"));
    m_convertButton->setText(tr("互转"));
    m_convertButton->setToolTip(
        tr("在文本与 HEX 之间互转：文本转 HEX，或把 HEX 反解为文本"));
    controls->addWidget(m_convertButton);

    controls->addStretch(1);

    layout->addLayout(controls);

    // 定时发送单独放一行，避免窄窗口下把整行控件的最小宽度撑大
    auto *timingRow = new QHBoxLayout();
    timingRow->setContentsMargins(0, 0, 0, 0);
    timingRow->setSpacing(8);

    m_timedCheck = new QCheckBox(tr("定时发送"), this);
    timingRow->addWidget(m_timedCheck);

    m_intervalSpin = new QSpinBox(this);
    m_intervalSpin->setRange(10, 3600000);
    m_intervalSpin->setSingleStep(100);
    m_intervalSpin->setValue(1000);
    m_intervalSpin->setSuffix(tr(" ms"));
    m_intervalSpin->setFixedWidth(100);
    timingRow->addWidget(m_intervalSpin);

    m_countSpin = new QSpinBox(this);
    m_countSpin->setRange(0, 1000000);
    m_countSpin->setSpecialValueText(tr("不限"));
    m_countSpin->setSuffix(tr(" 次"));
    m_countSpin->setFixedWidth(86);
    m_countSpin->setToolTip(tr("定时发送的次数，0 表示不限"));
    timingRow->addWidget(m_countSpin);

    timingRow->addStretch(1);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setProperty("muted", true);
    m_hintLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_hintLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    timingRow->addWidget(m_hintLabel, 2);

    layout->addLayout(timingRow);

    auto *inputRow = new QHBoxLayout();
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->setSpacing(8);

    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText(
        tr("输入要发送的数据。HEX 模式支持 AA 55 01 / AA5501 / 0xAA,0x55；Ctrl+Enter 发送"));
    m_input->setFont(Theme::monospaceFont(10));
    m_input->setMinimumHeight(62);
    inputRow->addWidget(m_input, 1);

    auto *buttonColumn = new QVBoxLayout();
    buttonColumn->setContentsMargins(0, 0, 0, 0);
    buttonColumn->setSpacing(6);

    m_sendButton = new QPushButton(tr("发送"), this);
    m_sendButton->setProperty("primary", true);
    m_sendButton->setMinimumSize(88, 32);
    buttonColumn->addWidget(m_sendButton);

    m_fileButton = new QPushButton(tr("发送文件"), this);
    m_fileButton->setMinimumHeight(26);
    m_fileButton->setToolTip(tr("以原始字节分块发送文件，带流控；发送过程中可取消"));
    buttonColumn->addWidget(m_fileButton);

    inputRow->addLayout(buttonColumn);
    layout->addLayout(inputRow);

    // ---- 快捷发送栏 ----
    m_quickSendBar = new QWidget(this);
    auto *quickLayout = new QHBoxLayout(m_quickSendBar);
    quickLayout->setContentsMargins(0, 0, 0, 0);
    quickLayout->setSpacing(6);

    auto *quickLabel = makeMutedLabel(tr("快捷"), m_quickSendBar);
    quickLayout->addWidget(quickLabel);

    m_quickSendSaveButton = new QToolButton(m_quickSendBar);
    m_quickSendSaveButton->setText(tr("保存当前"));
    m_quickSendSaveButton->setToolTip(
        tr("把当前输入框的内容保存为一条快捷发送"));
    quickLayout->addWidget(m_quickSendSaveButton);

    m_quickSendDeleteButton = new QToolButton(m_quickSendBar);
    m_quickSendDeleteButton->setText(tr("删除"));
    m_quickSendDeleteButton->setToolTip(tr("删除选中的快捷发送条目"));
    m_quickSendDeleteButton->setEnabled(false);
    quickLayout->addWidget(m_quickSendDeleteButton);

    quickLayout->addStretch(1);
    layout->addWidget(m_quickSendBar);

    // ---- 轮发序列栏 ----
    m_sequenceBar = new QWidget(this);
    auto *sequenceLayout = new QHBoxLayout(m_sequenceBar);
    sequenceLayout->setContentsMargins(0, 0, 0, 0);
    sequenceLayout->setSpacing(6);

    sequenceLayout->addWidget(makeMutedLabel(tr("轮发"), m_sequenceBar));

    m_sequenceModeCombo = new QComboBox(m_sequenceBar);
    m_sequenceModeCombo->addItem(tr("定时"), static_cast<int>(SequenceRunner::Mode::Interval));
    m_sequenceModeCombo->addItem(tr("应答后"), static_cast<int>(SequenceRunner::Mode::Reply));
    m_sequenceModeCombo->setFixedWidth(84);
    m_sequenceModeCombo->setToolTip(
        tr("定时：按组间隔逐条发送；\n"
           "应答后：发出一条后等待设备应答，收到数据才发送下一条"));
    sequenceLayout->addWidget(m_sequenceModeCombo);

    m_sequenceEditButton = new QToolButton(m_sequenceBar);
    m_sequenceEditButton->setText(tr("编辑序列…"));
    m_sequenceEditButton->setToolTip(tr("编辑多组轮发数据"));
    sequenceLayout->addWidget(m_sequenceEditButton);

    m_sequenceStartButton = new QToolButton(m_sequenceBar);
    m_sequenceStartButton->setText(tr("开始轮发"));
    m_sequenceStartButton->setToolTip(tr("开始 / 停止轮发序列"));
    sequenceLayout->addWidget(m_sequenceStartButton);

    m_sequenceStatusLabel = new QLabel(m_sequenceBar);
    m_sequenceStatusLabel->setProperty("muted", true);
    m_sequenceStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    sequenceLayout->addWidget(m_sequenceStatusLabel, 1);

    layout->addWidget(m_sequenceBar);

    auto *footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(8);

    m_historyCombo = new QComboBox(this);
    m_historyCombo->setMinimumWidth(160);
    m_historyCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_historyCombo->setToolTip(tr("最近 50 条发送内容"));
    footer->addWidget(m_historyCombo, 1);

    layout->addLayout(footer);
}

void SendPanel::restoreSettings()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("send"));
    const bool hex = s->value(QStringLiteral("hexMode"), false).toBool();
    const int encoding = s->value(QStringLiteral("encoding"),
                                  static_cast<int>(TextEncoding::Utf8)).toInt();
    const int lineEnding = s->value(QStringLiteral("lineEnding"), 0).toInt();
    // 旧版本存的是下拉框索引，新版本存的是 AppendMode 枚举值。
    // 两者在 None/Crc16Modbus/Crc16Ccitt/Sum8 上一致，
    // 但新增的 Xor8/Lrc8 只可能来自新版本，这里按 data 匹配更稳妥。
    const int checksum = s->value(QStringLiteral("checksum"), 0).toInt();
    m_intervalSpin->setValue(s->value(QStringLiteral("interval"), 1000).toInt());
    m_countSpin->setValue(s->value(QStringLiteral("count"), 0).toInt());
    m_history = s->value(QStringLiteral("history")).toStringList();
    const QString draft = s->value(QStringLiteral("draft")).toString();
    const QString quickJson = s->value(QStringLiteral("quickSends")).toString();
    const QString sequenceJson = s->value(QStringLiteral("sequence")).toString();
    const int sequenceMode = s->value(QStringLiteral("sequenceMode"), 0).toInt();
    s->endGroup();

    m_modeCombo->setCurrentIndex(hex ? 1 : 0);
    m_hexMode = hex;
    const int encodingIndex = m_encodingCombo->findData(encoding);
    m_encodingCombo->setCurrentIndex(encodingIndex >= 0 ? encodingIndex : 0);
    m_lineEndingCombo->setCurrentIndex(qBound(0, lineEnding, m_lineEndingCombo->count() - 1));
    const int checksumIndex = m_checksumCombo->findData(checksum);
    m_checksumCombo->setCurrentIndex(checksumIndex >= 0 ? checksumIndex : 0);
    m_input->setPlainText(draft);
    rebuildHistoryCombo();

    if (!quickJson.isEmpty())
        m_quickSends.fromJson(quickJson);
    rebuildQuickSendButtons();

    if (!sequenceJson.isEmpty())
        m_sequence.fromJson(sequenceJson);
    const int sequenceModeIndex = m_sequenceModeCombo->findData(sequenceMode);
    m_sequenceModeCombo->setCurrentIndex(sequenceModeIndex >= 0 ? sequenceModeIndex : 0);
    m_sequenceRunner.setMode(
        static_cast<SequenceRunner::Mode>(m_sequenceModeCombo->currentData().toInt()));
    updateSequenceStatus();
}

void SendPanel::saveSettings()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("send"));
    s->setValue(QStringLiteral("hexMode"), m_hexMode);
    s->setValue(QStringLiteral("encoding"), m_encodingCombo->currentData().toInt());
    s->setValue(QStringLiteral("lineEnding"), m_lineEndingCombo->currentData().toInt());
    s->setValue(QStringLiteral("checksum"), m_checksumCombo->currentData().toInt());
    s->setValue(QStringLiteral("interval"), m_intervalSpin->value());
    s->setValue(QStringLiteral("count"), m_countSpin->value());
    s->setValue(QStringLiteral("history"), m_history);
    s->setValue(QStringLiteral("draft"), m_input->toPlainText());
    s->setValue(QStringLiteral("quickSends"), m_quickSends.toJson());
    s->setValue(QStringLiteral("sequence"), m_sequence.toJson());
    s->setValue(QStringLiteral("sequenceMode"), m_sequenceModeCombo->currentData().toInt());
    s->endGroup();
}

void SendPanel::setPortOpen(bool open)
{
    m_portOpen = open;
    if (!open) {
        if (m_timedCheck->isChecked()) {
            QSignalBlocker blocker(m_timedCheck);
            m_timedCheck->setChecked(false);
            m_timedTimer->stop();
        }
        // 端口断开时必须停掉轮发，否则会对着一个不存在的串口空转
        if (m_sequenceRunning) {
            m_sequenceTimer->stop();
            m_sequenceRunner.stop();
            m_sequenceRunning = false;
            m_sequenceAwaitingReply = false;
            m_sequenceStartButton->setText(tr("开始轮发"));
        }
        m_fileSending = false;
    }
    updateControls();
    updateSequenceStatus();
}

bool SendPanel::validateInput(QString *error) const
{
    const QString text = m_input->toPlainText();
    if (text.isEmpty()) {
        *error = tr("发送内容为空");
        return false;
    }

    if (m_hexMode) {
        QString hexError;
        HexCodec::parseHex(text, &hexError);
        if (!hexError.isEmpty()) {
            *error = tr("HEX 错误：%1").arg(hexError);
            return false;
        }
        return true;
    }

    const TextEncoding enc = static_cast<TextEncoding>(m_encodingCombo->currentData().toInt());
    if (enc == TextEncoding::Ascii) {
        for (int i = 0; i < text.size(); ++i) {
            if (text.at(i).unicode() > 0x7F) {
                *error = tr("ASCII 编码无法表示非 ASCII 字符（位置 %1）").arg(i + 1);
                return false;
            }
        }
    } else if (QTextCodec *codec = HexCodec::codecFor(enc)) {
        if (!codec->canEncode(text)) {
            *error = tr("%1 编码无法表示输入中的全部字符").arg(textEncodingName(enc));
            return false;
        }
    }
    return true;
}

bool SendPanel::buildRawPayload(QByteArray *payload, QString *error) const
{
    const QString text = m_input->toPlainText();
    if (text.isEmpty()) {
        *error = tr("发送内容为空");
        return false;
    }

    if (m_hexMode) {
        QString hexError;
        const QByteArray parsed = HexCodec::parseHex(text, &hexError);
        if (!hexError.isEmpty()) {
            *error = tr("HEX 错误：%1").arg(hexError);
            return false;
        }
        *payload = parsed;
    } else {
        const TextEncoding enc = static_cast<TextEncoding>(m_encodingCombo->currentData().toInt());
        *payload = HexCodec::encode(text, enc);
    }
    return true;
}

void SendPanel::onConvertClicked()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) {
        setHint(tr("没有可转换的内容"), true);
        return;
    }

    if (m_hexMode) {
        // HEX → 文本
        QString hexError;
        const QByteArray raw = HexCodec::parseHex(text, &hexError);
        if (!hexError.isEmpty()) {
            setHint(tr("HEX 错误：%1").arg(hexError), true);
            return;
        }
        const TextEncoding enc = static_cast<TextEncoding>(m_encodingCombo->currentData().toInt());
        const QString decoded = HexCodec::decode(raw, enc);
        m_modeCombo->setCurrentIndex(0);
        m_input->setPlainText(decoded);
        setHint(tr("已把 HEX 转换为文本（%1 字节）").arg(raw.size()), false);
    } else {
        // 文本 → HEX
        const TextEncoding enc = static_cast<TextEncoding>(m_encodingCombo->currentData().toInt());
        const QByteArray raw = HexCodec::encode(text, enc);
        m_modeCombo->setCurrentIndex(1);
        m_input->setPlainText(HexCodec::formatHex(raw));
        setHint(tr("已把文本转换为 HEX（%1 字节）").arg(raw.size()), false);
    }
}

QuickSendItem SendPanel::currentQuickSendItem() const
{
    QuickSendItem item;
    item.payload = m_input->toPlainText().trimmed();
    item.hex = m_hexMode;
    return item;
}

void SendPanel::onQuickSendSave()
{
    QuickSendItem item = currentQuickSendItem();
    if (!item.isValid()) {
        setHint(tr("输入框内容为空，无法保存快捷发送"), true);
        return;
    }

    // 名称取输入内容的前若干字符；重复时覆盖
    item.name = item.payload.left(12);
    if (item.hex)
        item.name = item.payload.left(12);

    if (!m_quickSends.add(item)) {
        setHint(tr("保存快捷发送失败"), true);
        return;
    }

    rebuildQuickSendButtons();
    saveSettings();
    setHint(tr("已保存快捷发送：%1").arg(item.name), false);
}

void SendPanel::onQuickSendDelete()
{
    // 删除最后点击的快捷按钮
    for (int i = 0; i < m_quickSendButtons.size(); ++i) {
        QPushButton *button = m_quickSendButtons.at(i);
        if (!button->property("selected").toBool())
            continue;
        if (m_quickSends.remove(button->property("name").toString())) {
            rebuildQuickSendButtons();
            saveSettings();
            setHint(tr("已删除快捷发送：%1").arg(button->property("name").toString()), false);
        }
        return;
    }
}

void SendPanel::onQuickSendClicked()
{
    auto *button = qobject_cast<QPushButton *>(sender());
    if (!button)
        return;

    const QString name = button->property("name").toString();
    const QuickSendItem *item = m_quickSends.find(name);
    if (!item)
        return;

    // 标记选中状态，供"删除"按钮使用
    for (QPushButton *other : m_quickSendButtons)
        other->setProperty("selected", other == button);
    m_quickSendDeleteButton->setEnabled(true);

    // 点击即发送该条目
    QByteArray payload;
    if (item->hex) {
        QString hexError;
        payload = HexCodec::parseHex(item->payload, &hexError);
        if (!hexError.isEmpty()) {
            setHint(tr("快捷发送 %1 的 HEX 无效：%2").arg(name, hexError), true);
            return;
        }
    } else {
        const TextEncoding enc = static_cast<TextEncoding>(m_encodingCombo->currentData().toInt());
        payload = HexCodec::encode(item->payload, enc);
    }

    if (!m_portOpen) {
        setHint(tr("串口未打开"), true);
        return;
    }

    emit dataToSend(payload);
    emit statusMessage(tr("已发送 %1 字节（快捷：%2）").arg(payload.size()).arg(name));
}

void SendPanel::rebuildQuickSendButtons()
{
    // 清掉旧的快捷按钮（保留"快捷"标签与保存/删除按钮）
    for (QPushButton *button : m_quickSendButtons) {
        if (auto *layout = qobject_cast<QHBoxLayout *>(m_quickSendBar->layout()))
            layout->removeWidget(button);
        button->deleteLater();
    }
    m_quickSendButtons.clear();
    m_quickSendDeleteButton->setEnabled(false);

    auto *layout = qobject_cast<QHBoxLayout *>(m_quickSendBar->layout());
    if (!layout)
        return;

    // 重新插入到"删除"按钮之后：找到删除按钮在布局中的位置
    int insertIndex = layout->count();
    for (int i = 0; i < layout->count(); ++i) {
        QLayoutItem *item = layout->itemAt(i);
        QWidget *widget = item ? item->widget() : nullptr;
        if (widget == m_quickSendDeleteButton) {
            insertIndex = i + 1;
            break;
        }
    }

    for (const QuickSendItem &item : m_quickSends.items()) {
        auto *button = new QPushButton(item.name, m_quickSendBar);
        button->setProperty("name", item.name);
        button->setProperty("selected", false);
        button->setToolTip(tr("点击立即发送：%1").arg(item.payload));
        button->setMaximumWidth(140);
        connect(button, &QPushButton::clicked, this, &SendPanel::onQuickSendClicked);
        layout->insertWidget(insertIndex++, button);
        m_quickSendButtons.append(button);
    }
}

bool SendPanel::buildPayload(QByteArray *payload, QString *error) const
{
    QByteArray data;
    if (!buildRawPayload(&data, error))
        return false;

    const auto checksum = static_cast<CrcCalculator::AppendMode>(
        m_checksumCombo->currentData().toInt());
    data = CrcCalculator::appendChecksum(data, checksum);

    if (!m_hexMode) {
        switch (m_lineEndingCombo->currentData().toInt()) {
        case 1: data += "\r\n"; break;
        case 2: data += '\n'; break;
        case 3: data += '\r'; break;
        default: break;
        }
    }

    if (data.isEmpty()) {
        *error = tr("发送内容为空");
        return false;
    }

    *payload = data;
    return true;
}

void SendPanel::updateControls()
{
    QString error;
    const bool valid = validateInput(&error);

    m_sendButton->setEnabled(m_portOpen && valid && !m_fileSending);
    m_sendButton->setToolTip(m_portOpen ? tr("发送当前内容（Ctrl+Enter）")
                                        : tr("请先打开串口"));
    m_fileButton->setEnabled(m_portOpen || m_fileSending);
    m_fileButton->setText(m_fileSending ? tr("取消发送") : tr("发送文件"));
    m_timedCheck->setEnabled(m_portOpen && !m_fileSending);
    m_intervalSpin->setEnabled(m_timedCheck->isChecked() && !m_fileSending);
    m_countSpin->setEnabled(m_timedCheck->isChecked() && !m_fileSending);

    if (!valid) {
        if (m_input->toPlainText().isEmpty()) {
            setHint(m_portOpen ? tr("输入要发送的内容") : tr("串口未打开"), false);
        } else {
            setHint(error, true);
        }
        return;
    }

    QByteArray payload;
    QString buildError;
    if (!buildPayload(&payload, &buildError)) {
        setHint(buildError, true);
        return;
    }

    QString preview = HexCodec::formatHex(payload.left(24));
    if (payload.size() > 24)
        preview += QStringLiteral(" …");
    QString hint = tr("将发送 %1 字节 · %2").arg(payload.size()).arg(preview);
    if (!m_portOpen)
        hint = tr("串口未打开");
    setHint(hint, false);
}

void SendPanel::setHint(const QString &message, bool isError)
{
    if (m_hintLabel->text() == message
        && m_hintLabel->property("error").toBool() == isError) {
        return;
    }
    m_hintLabel->setText(message);
    m_hintLabel->setToolTip(message);
    m_hintLabel->setProperty("error", isError);
    m_hintLabel->setProperty("muted", !isError);
    repolish(m_hintLabel);
}

bool SendPanel::sendCurrent(bool addToHistory)
{
    if (!m_portOpen) {
        setHint(tr("串口未打开"), true);
        return false;
    }
    if (m_fileSending) {
        setHint(tr("文件发送中，请等待或取消"), true);
        return false;
    }

    QByteArray payload;
    QString error;
    if (!buildPayload(&payload, &error)) {
        setHint(error, true);
        return false;
    }

    emit dataToSend(payload);
    if (addToHistory)
        addHistory(m_input->toPlainText());
    emit statusMessage(tr("已发送 %1 字节").arg(payload.size()));
    return true;
}

void SendPanel::onSendClicked()
{
    sendCurrent(true);
}

void SendPanel::onModeChanged()
{
    m_hexMode = m_modeCombo->currentData().toBool();
    m_encodingCombo->setEnabled(!m_hexMode);
    m_lineEndingCombo->setEnabled(!m_hexMode);
    saveSettings();
    updateControls();
}

void SendPanel::onTimedToggled(bool enabled)
{
    if (!enabled) {
        m_timedTimer->stop();
        updateControls();
        emit statusMessage(tr("定时发送已停止"));
        return;
    }

    if (!m_portOpen) {
        QSignalBlocker blocker(m_timedCheck);
        m_timedCheck->setChecked(false);
        setHint(tr("串口未打开"), true);
        return;
    }

    QString error;
    if (!validateInput(&error)) {
        QSignalBlocker blocker(m_timedCheck);
        m_timedCheck->setChecked(false);
        setHint(error, true);
        return;
    }

    m_remainingSends = m_countSpin->value();
    m_timedTimer->start(m_intervalSpin->value());
    updateControls();
    emit statusMessage(tr("定时发送已启动：间隔 %1 ms，次数 %2")
                           .arg(m_intervalSpin->value())
                           .arg(m_remainingSends > 0 ? QString::number(m_remainingSends)
                                                     : tr("不限")));
}

void SendPanel::onTimedTimeout()
{
    if (!sendCurrent(false)) {
        QSignalBlocker blocker(m_timedCheck);
        m_timedCheck->setChecked(false);
        m_timedTimer->stop();
        updateControls();
        emit statusMessage(tr("定时发送已停止（发送失败）"));
        return;
    }
    if (m_remainingSends > 0) {
        --m_remainingSends;
        if (m_remainingSends == 0) {
            QSignalBlocker blocker(m_timedCheck);
            m_timedCheck->setChecked(false);
            m_timedTimer->stop();
            updateControls();
            emit statusMessage(tr("定时发送完成"));
        }
    }
}

void SendPanel::onIntervalChanged(int value)
{
    if (m_timedTimer->isActive())
        m_timedTimer->setInterval(value);
    saveSettings();
}

void SendPanel::onTextChanged()
{
    updateControls();
}

void SendPanel::onSequenceEdit()
{
    SequenceDialog dialog(this);
    dialog.setSequence(m_sequence);
    dialog.exec();
    m_sequence = dialog.sequence();
    m_sequenceRunner.setGroups(m_sequence.groups());
    saveSettings();
    updateSequenceStatus();
}

void SendPanel::onSequenceModeChanged(int index)
{
    Q_UNUSED(index)
    m_sequenceRunner.setMode(
        static_cast<SequenceRunner::Mode>(m_sequenceModeCombo->currentData().toInt()));
    saveSettings();
    updateSequenceStatus();
}

void SendPanel::onSequenceStart()
{
    if (m_sequenceRunning) {
        stopSequence();
        return;
    }
    startSequence();
}

void SendPanel::onSequenceStop()
{
    stopSequence();
}

bool SendPanel::startSequence()
{
    if (m_sequence.isEmpty()) {
        setHint(tr("请先编辑轮发序列"), true);
        return false;
    }
    if (!m_portOpen) {
        setHint(tr("串口未打开"), true);
        return false;
    }

    m_sequenceRunner.setGroups(m_sequence.groups());
    m_sequenceRunner.setMode(
        static_cast<SequenceRunner::Mode>(m_sequenceModeCombo->currentData().toInt()));
    if (!m_sequenceRunner.start()) {
        setHint(tr("轮发序列中没有可用条目"), true);
        return false;
    }

    m_sequenceRunning = true;
    m_sequenceAwaitingReply = false;
    m_sequenceStartButton->setText(tr("停止轮发"));
    updateSequenceStatus();

    // 先发第一条
    sendNextSequenceItem();

    if (m_sequenceRunner.mode() == SequenceRunner::Mode::Interval) {
        m_sequenceTimer->start(m_sequenceRunner.currentInterval());
    }
    return true;
}

void SendPanel::stopSequence()
{
    m_sequenceTimer->stop();
    m_sequenceRunner.stop();
    m_sequenceRunning = false;
    m_sequenceAwaitingReply = false;
    m_sequenceStartButton->setText(tr("开始轮发"));
    updateSequenceStatus();
    emit statusMessage(tr("轮发已停止"));
}

void SendPanel::sendNextSequenceItem()
{
    if (!m_sequenceRunning)
        return;

    QByteArray payload;
    QString label;
    if (!m_sequenceRunner.currentPayload(&payload, &label)) {
        stopSequence();
        return;
    }

    emit dataToSend(payload);
    m_sequenceRunner.advance();
    updateSequenceStatus();
    emit statusMessage(tr("轮发已发送：%1（%2 字节）").arg(label).arg(payload.size()));

    if (m_sequenceRunner.mode() == SequenceRunner::Mode::Reply) {
        // 等收到应答后再发下一条
        m_sequenceAwaitingReply = true;
        updateSequenceStatus();
    } else {
        // 定时模式下按当前组间隔重设定时器
        m_sequenceTimer->start(m_sequenceRunner.currentInterval());
    }
}

void SendPanel::onDataReceived()
{
    if (!m_sequenceRunning || !m_sequenceAwaitingReply)
        return;
    if (m_sequenceRunner.mode() != SequenceRunner::Mode::Reply)
        return;

    // 收到应答，发送下一条
    m_sequenceAwaitingReply = false;
    sendNextSequenceItem();
}

void SendPanel::updateSequenceStatus()
{
    if (!m_sequenceStatusLabel)
        return;

    if (m_sequence.isEmpty()) {
        m_sequenceStatusLabel->setText(tr("未配置序列"));
        m_sequenceStartButton->setEnabled(m_portOpen);
        m_sequenceStartButton->setText(tr("开始轮发"));
        return;
    }

    m_sequenceStartButton->setEnabled(m_portOpen);

    if (!m_sequenceRunning) {
        m_sequenceStatusLabel->setText(
            tr("%1 组 · %2 条")
                .arg(m_sequence.count())
                .arg([this] {
                    int total = 0;
                    for (const SequenceGroup &group : m_sequence.groups())
                        total += group.items.size();
                    return total;
                }()));
        m_sequenceStartButton->setText(tr("开始轮发"));
        return;
    }

    const bool waiting = m_sequenceAwaitingReply;
    m_sequenceStatusLabel->setText(
        waiting ? tr("等待应答…（%1）").arg(m_sequenceRunner.currentGroupName())
                : tr("进行中：%1").arg(m_sequenceRunner.currentLabel()));
    m_sequenceStartButton->setText(tr("停止轮发"));
}

void SendPanel::onHistoryActivated(int index)
{
    if (index <= 0)
        return;
    const QString text = m_historyCombo->itemData(index).toString();
    m_input->setPlainText(text);
    QSignalBlocker blocker(m_historyCombo);
    m_historyCombo->setCurrentIndex(0);
}

void SendPanel::addHistory(const QString &text)
{
    if (text.trimmed().isEmpty())
        return;
    m_history.removeAll(text);
    m_history.prepend(text);
    while (m_history.size() > 50)
        m_history.removeLast();
    rebuildHistoryCombo();
    saveSettings();
}

void SendPanel::rebuildHistoryCombo()
{
    QSignalBlocker blocker(m_historyCombo);
    m_historyCombo->clear();
    m_historyCombo->addItem(tr("历史记录…"), QString());
    for (const QString &entry : m_history) {
        QString display = entry;
        const int newline = display.indexOf(QLatin1Char('\n'));
        if (newline >= 0)
            display = display.left(newline) + QStringLiteral(" …");
        if (display.size() > 60)
            display = display.left(60) + QStringLiteral("…");
        m_historyCombo->addItem(display, entry);
    }
    m_historyCombo->setCurrentIndex(0);
}

void SendPanel::onFileClicked()
{
    if (m_fileSending) {
        emit cancelFileSendRequested();
        setHint(tr("正在取消文件发送…"), false);
        return;
    }

    if (!m_portOpen) {
        setHint(tr("串口未打开"), true);
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, tr("选择要发送的文件"), SettingsManager::lastDirectory(),
        tr("所有文件 (*)"));
    if (path.isEmpty())
        return;

    SettingsManager::setLastDirectory(QFileInfo(path).absolutePath());

    if (m_timedCheck->isChecked()) {
        QSignalBlocker blocker(m_timedCheck);
        m_timedCheck->setChecked(false);
        m_timedTimer->stop();
    }

    m_fileSending = true;
    m_fileName = QFileInfo(path).fileName();
    const qint64 size = QFileInfo(path).size();
    updateControls();
    emit statusMessage(tr("开始发送文件 %1（%2 字节）").arg(m_fileName).arg(size));
    emit fileSendRequested(path);
}

void SendPanel::setFileProgress(qint64 sentBytes, qint64 totalBytes)
{
    if (totalBytes <= 0)
        return;
    const int percent = static_cast<int>(sentBytes * 100 / totalBytes);
    emit statusMessage(tr("发送文件 %1：%2 / %3 字节（%4%）")
                           .arg(m_fileName)
                           .arg(sentBytes)
                           .arg(totalBytes)
                           .arg(percent));
}

void SendPanel::onFileSendFinished(bool success, const QString &errorMessage)
{
    m_fileSending = false;
    updateControls();
    if (success)
        emit statusMessage(tr("文件发送完成"));
    else {
        setHint(tr("文件发送失败：%1").arg(errorMessage), true);
        emit statusMessage(tr("文件发送失败：%1").arg(errorMessage));
    }
}

void SendPanel::onTxBackpressure(qint64 queuedBytes)
{
    setHint(tr("串口写缓冲积压（%1 KB），已跳过本次发送").arg(queuedBytes / 1024), true);
    emit statusMessage(tr("串口写缓冲积压，已跳过本次发送"));
    QTimer::singleShot(2500, this, [this] { updateControls(); });
}
