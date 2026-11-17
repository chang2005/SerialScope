#include "ui/dialogs/CrcToolDialog.h"

#include "core/parser/HexCodec.h"
#include "utils/CrcCalculator.h"

#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
enum Algorithm {
    Crc8 = 0,
    Crc16Modbus,
    Crc16Ccitt,
    Crc32,
    Sum8,
    Xor8,
};
}

CrcToolDialog::CrcToolDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("CRC / 校验工具"));
    setMinimumSize(520, 360);
    setupUi();
    updateResult();
}

void CrcToolDialog::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    auto *form = new QFormLayout();

    m_algorithmCombo = new QComboBox(this);
    m_algorithmCombo->addItem(QStringLiteral("CRC-8/ATM (poly 0x07)"), Crc8);
    m_algorithmCombo->addItem(QStringLiteral("CRC-16/MODBUS"), Crc16Modbus);
    m_algorithmCombo->addItem(QStringLiteral("CRC-16/CCITT-FALSE"), Crc16Ccitt);
    m_algorithmCombo->addItem(QStringLiteral("CRC-32/IEEE"), Crc32);
    m_algorithmCombo->addItem(QStringLiteral("Sum8 (累加和)"), Sum8);
    m_algorithmCombo->addItem(QStringLiteral("Xor8 (异或)"), Xor8);
    form->addRow(tr("算法"), m_algorithmCombo);

    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText(tr("输入 HEX 数据，例如：01 03 00 00 00 02"));
    m_input->setMinimumHeight(90);
    form->addRow(tr("输入数据"), m_input);

    auto *resultRow = new QWidget(this);
    auto *resultLayout = new QHBoxLayout(resultRow);
    resultLayout->setContentsMargins(0, 0, 0, 0);
    m_resultEdit = new QLineEdit(resultRow);
    m_resultEdit->setReadOnly(true);
    m_resultEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    resultLayout->addWidget(m_resultEdit, 1);
    auto *copyButton = new QPushButton(tr("复制"), resultRow);
    resultLayout->addWidget(copyButton);
    connect(copyButton, &QPushButton::clicked, this, &CrcToolDialog::copyResult);
    form->addRow(tr("计算结果"), resultRow);

    m_detailLabel = new QLabel(this);
    m_detailLabel->setProperty("muted", true);
    form->addRow(QString(), m_detailLabel);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setProperty("error", true);
    m_errorLabel->setWordWrap(true);
    form->addRow(QString(), m_errorLabel);

    layout->addLayout(form);

    auto *hint = new QLabel(tr("提示：输入中的空格、逗号、0x 前缀和换行都会被忽略。"), this);
    hint->setProperty("muted", true);
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::clicked, this, &QDialog::accept);
    layout->addWidget(buttons);

    connect(m_input, &QPlainTextEdit::textChanged, this, &CrcToolDialog::updateResult);
    connect(m_algorithmCombo, &QComboBox::currentIndexChanged, this,
            &CrcToolDialog::updateResult);
}

void CrcToolDialog::updateResult()
{
    QString error;
    const QByteArray data = HexCodec::parseHex(m_input->toPlainText(), &error);

    if (!error.isEmpty()) {
        m_errorLabel->setText(tr("HEX 错误：%1").arg(error));
        m_errorLabel->show();
        m_resultEdit->clear();
        m_detailLabel->clear();
        return;
    }

    m_errorLabel->hide();
    m_detailLabel->setText(tr("数据长度：%1 字节").arg(data.size()));
    if (data.isEmpty()) {
        m_resultEdit->clear();
        return;
    }

    const Algorithm algorithm = static_cast<Algorithm>(
        m_algorithmCombo->currentData().toInt());
    QString result;
    QString detail;

    switch (algorithm) {
    case Crc8: {
        const quint8 crc = CrcCalculator::crc8(data);
        result = QStringLiteral("0x%1").arg(crc, 2, 16, QLatin1Char('0')).toUpper();
        detail = tr("十进制：%1").arg(crc);
        break;
    }
    case Crc16Modbus: {
        const quint16 crc = CrcCalculator::crc16Modbus(data);
        result = QStringLiteral("0x%1").arg(crc, 4, 16, QLatin1Char('0')).toUpper();
        detail = tr("追加字节（低字节在前）：%1 %2")
                     .arg(crc & 0xFF, 2, 16, QLatin1Char('0'))
                     .arg((crc >> 8) & 0xFF, 2, 16, QLatin1Char('0'))
                     .toUpper();
        break;
    }
    case Crc16Ccitt: {
        const quint16 crc = CrcCalculator::crc16CcittFalse(data);
        result = QStringLiteral("0x%1").arg(crc, 4, 16, QLatin1Char('0')).toUpper();
        detail = tr("追加字节（高字节在前）：%1 %2")
                     .arg((crc >> 8) & 0xFF, 2, 16, QLatin1Char('0'))
                     .arg(crc & 0xFF, 2, 16, QLatin1Char('0'))
                     .toUpper();
        break;
    }
    case Crc32: {
        const quint32 crc = CrcCalculator::crc32(data);
        result = QStringLiteral("0x%1").arg(crc, 8, 16, QLatin1Char('0')).toUpper();
        break;
    }
    case Sum8: {
        const quint8 sum = CrcCalculator::sum8(data);
        result = QStringLiteral("0x%1").arg(sum, 2, 16, QLatin1Char('0')).toUpper();
        detail = tr("十进制：%1").arg(sum);
        break;
    }
    case Xor8: {
        const quint8 value = CrcCalculator::xor8(data);
        result = QStringLiteral("0x%1").arg(value, 2, 16, QLatin1Char('0')).toUpper();
        detail = tr("十进制：%1").arg(value);
        break;
    }
    }

    m_resultEdit->setText(result);
    m_detailLabel->setText(tr("数据长度：%1 字节    %2").arg(data.size()).arg(detail));
}

void CrcToolDialog::copyResult()
{
    if (!m_resultEdit->text().isEmpty())
        QGuiApplication::clipboard()->setText(m_resultEdit->text());
}
