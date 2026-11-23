#include "ui/dialogs/EncodingFixDialog.h"

#include "core/parser/EncodingFixer.h"
#include "core/parser/HexCodec.h"

#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>

EncodingFixDialog::EncodingFixDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    updatePreview();
}

void EncodingFixDialog::setupUi()
{
    setWindowTitle(tr("编码转换 / 乱码修复"));
    resize(760, 560);

    auto *layout = new QVBoxLayout(this);

    auto *hint = new QLabel(
        tr("乱码通常源于编码不匹配：设备发 GBK 字节、工具按 UTF-8 解读就会显示乱码。"
           "文本模式下选对「原始编码」与「目标编码」即可还原；"
           "若乱码里已有替换字符（�），说明字节已丢失，请改用 HEX 模式粘贴原始字节。"),
        this);
    hint->setWordWrap(true);
    hint->setProperty("muted", true);
    layout->addWidget(hint);

    auto *modeGroup = new QGroupBox(tr("输入方式"), this);
    auto *modeRow = new QHBoxLayout(modeGroup);
    auto *textMode = new QRadioButton(tr("乱码文本"), modeGroup);
    auto *hexMode = new QRadioButton(tr("HEX 原始字节"), modeGroup);
    textMode->setChecked(true);
    modeRow->addWidget(textMode);
    modeRow->addWidget(hexMode);
    modeRow->addStretch(1);
    layout->addWidget(modeGroup);

    auto *form = new QFormLayout();

    m_input = new QPlainTextEdit(this);
    m_input->setMinimumHeight(110);
    m_input->setPlaceholderText(tr("在此粘贴乱码文本…"));
    form->addRow(tr("输入"), m_input);

    auto *encodingRow = new QWidget(this);
    auto *encodingLayout = new QHBoxLayout(encodingRow);
    encodingLayout->setContentsMargins(0, 0, 0, 0);

    auto addEncodings = [](QComboBox *combo) {
        combo->addItem(EncodingFixer::encodingName(EncodingFixer::Encoding::Utf8),
                       static_cast<int>(EncodingFixer::Encoding::Utf8));
        combo->addItem(EncodingFixer::encodingName(EncodingFixer::Encoding::Gbk),
                       static_cast<int>(EncodingFixer::Encoding::Gbk));
        combo->addItem(EncodingFixer::encodingName(EncodingFixer::Encoding::Latin1),
                       static_cast<int>(EncodingFixer::Encoding::Latin1));
        combo->addItem(EncodingFixer::encodingName(EncodingFixer::Encoding::Ascii),
                       static_cast<int>(EncodingFixer::Encoding::Ascii));
    };

    m_sourceCombo = new QComboBox(encodingRow);
    addEncodings(m_sourceCombo);
    m_sourceCombo->setCurrentIndex(1); // 默认 GBK：设备端最常见的中文编码
    encodingLayout->addWidget(new QLabel(tr("原始编码："), encodingRow));
    encodingLayout->addWidget(m_sourceCombo);

    m_targetCombo = new QComboBox(encodingRow);
    addEncodings(m_targetCombo);
    encodingLayout->addWidget(new QLabel(tr("目标编码："), encodingRow));
    encodingLayout->addWidget(m_targetCombo);
    encodingLayout->addStretch(1);
    form->addRow(tr("编码"), encodingRow);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMinimumHeight(110);
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    form->addRow(tr("结果"), m_output);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setProperty("muted", true);
    m_statusLabel->setWordWrap(true);
    form->addRow(QString(), m_statusLabel);

    layout->addLayout(form);

    auto *candidateBox = new QGroupBox(tr("试解候选（HEX 模式下按可读性排序，点击采纳）"), this);
    auto *candidateLayout = new QVBoxLayout(candidateBox);
    m_candidateTable = new QTableWidget(0, 4, candidateBox);
    m_candidateTable->setHorizontalHeaderLabels(
        {tr("编码"), tr("评分"), tr("替换字符"), tr("预览")});
    m_candidateTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_candidateTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_candidateTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_candidateTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_candidateTable->verticalHeader()->setVisible(false);
    m_candidateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_candidateTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    candidateLayout->addWidget(m_candidateTable);
    layout->addWidget(candidateBox, 1);

    auto *buttonRow = new QHBoxLayout();
    auto *copyButton = new QPushButton(tr("复制结果"), this);
    buttonRow->addWidget(copyButton);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::clicked, this, &QDialog::accept);
    layout->addWidget(buttons);

    connect(copyButton, &QPushButton::clicked, this, &EncodingFixDialog::copyResult);
    connect(textMode, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked)
            m_hexMode = false;
        onModeChanged();
    });
    connect(hexMode, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked)
            m_hexMode = true;
        onModeChanged();
    });
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this,
            &EncodingFixDialog::updatePreview);
    connect(m_targetCombo, &QComboBox::currentIndexChanged, this,
            &EncodingFixDialog::updatePreview);
    connect(m_input, &QPlainTextEdit::textChanged, this, &EncodingFixDialog::updatePreview);
    connect(m_candidateTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (row < 0 || row >= m_candidateTable->rowCount())
            return;
        QTableWidgetItem *item = m_candidateTable->item(row, 0);
        if (!item)
            return;
        const int encoding = item->data(Qt::UserRole).toInt();
        m_targetCombo->setCurrentIndex(m_targetCombo->findData(encoding));
    });
}

void EncodingFixDialog::onModeChanged()
{
    m_input->setPlaceholderText(m_hexMode
                                    ? tr("粘贴 HEX 原始字节，例如：D6 D0 CE C4")
                                    : tr("在此粘贴乱码文本…"));
    updatePreview();
}

bool EncodingFixDialog::isHexMode() const
{
    return m_hexMode;
}

QByteArray EncodingFixDialog::collectBytes(bool *ok) const
{
    if (ok)
        *ok = true;
    const QString text = m_input->toPlainText();
    if (text.trimmed().isEmpty())
        return QByteArray();

    if (isHexMode()) {
        QString error;
        const QByteArray bytes = HexCodec::parseHex(text, &error);
        if (!error.isEmpty() && ok)
            *ok = false;
        return bytes;
    }

    // 文本模式：把乱码文本按「原始编码」逆转回字节
    const auto src = static_cast<EncodingFixer::Encoding>(
        m_sourceCombo->currentData().toInt());
    return EncodingFixer::encodeText(text, src);
}

void EncodingFixDialog::updatePreview()
{
    bool ok = true;
    const QByteArray bytes = collectBytes(&ok);
    if (!ok) {
        m_output->clear();
        m_statusLabel->setText(tr("HEX 解析失败，请检查输入格式"));
        refreshCandidates();
        return;
    }
    if (bytes.isEmpty()) {
        m_output->clear();
        m_statusLabel->clear();
        refreshCandidates();
        return;
    }

    const auto src = static_cast<EncodingFixer::Encoding>(
        m_sourceCombo->currentData().toInt());
    const auto dst = static_cast<EncodingFixer::Encoding>(
        m_targetCombo->currentData().toInt());

    QString result;
    if (isHexMode()) {
        // HEX 模式：直接按目标编码解读原始字节
        result = EncodingFixer::decodeBytes(bytes, dst);
    } else {
        result = EncodingFixer::fix(m_input->toPlainText(), src, dst);
    }
    m_output->setPlainText(result);

    int replacements = 0;
    const int score = EncodingFixer::readabilityScore(result, &replacements);
    m_statusLabel->setText(
        tr("字节数：%1    可读性评分：%2    替换字符：%3")
            .arg(bytes.size())
            .arg(score)
            .arg(replacements));

    refreshCandidates();
}

void EncodingFixDialog::refreshCandidates()
{
    m_candidateTable->clearContents();
    m_candidateTable->setRowCount(0);
    m_candidateTable->setEnabled(isHexMode());
    if (!isHexMode())
        return;

    bool ok = true;
    const QByteArray bytes = collectBytes(&ok);
    if (!ok || bytes.isEmpty())
        return;

    const QVector<EncodingFixer::Candidate> candidates =
        EncodingFixer::candidates(bytes);
    m_candidateTable->setRowCount(candidates.size());
    for (int row = 0; row < candidates.size(); ++row) {
        const auto &candidate = candidates.at(row);
        auto *nameItem = new QTableWidgetItem(
            EncodingFixer::encodingName(candidate.encoding));
        nameItem->setData(Qt::UserRole, static_cast<int>(candidate.encoding));
        m_candidateTable->setItem(row, 0, nameItem);
        m_candidateTable->setItem(row, 1,
                                  new QTableWidgetItem(QString::number(candidate.score)));
        m_candidateTable->setItem(row, 2,
                                  new QTableWidgetItem(QString::number(candidate.replacementChars)));

        QString preview = candidate.text;
        preview.replace(QLatin1Char('\n'), QLatin1Char(' '));
        preview.replace(QLatin1Char('\r'), QLatin1Char(' '));
        if (preview.size() > 120)
            preview = preview.left(120) + QStringLiteral("…");
        m_candidateTable->setItem(row, 3, new QTableWidgetItem(preview));
    }
}

void EncodingFixDialog::copyResult()
{
    const QString text = m_output->toPlainText();
    if (!text.isEmpty())
        QGuiApplication::clipboard()->setText(text);
}
