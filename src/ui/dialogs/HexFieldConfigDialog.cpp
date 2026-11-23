#include "ui/dialogs/HexFieldConfigDialog.h"

#include "core/parser/HexCodec.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

const char *const kFieldTypes[] = {"int8", "uint8", "int16", "uint16",
                                   "int32", "uint32", "float32"};

QComboBox *createTypeCombo(QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    for (const char *type : kFieldTypes)
        combo->addItem(QString::fromLatin1(type));
    combo->setCurrentText(QStringLiteral("uint16"));
    return combo;
}

} // namespace

HexFieldConfigDialog::HexFieldConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("HEX 波形协议配置"));
    setMinimumWidth(620);
    setupUi();
}

void HexFieldConfigDialog::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);

    auto *frameGroup = new QGroupBox(tr("帧格式"), this);
    auto *form = new QFormLayout(frameGroup);
    form->setLabelAlignment(Qt::AlignRight);

    m_headerEdit = new QLineEdit(frameGroup);
    m_headerEdit->setPlaceholderText(QStringLiteral("AA 55"));
    m_headerEdit->setToolTip(tr("帧头字节，支持空格分隔或连续输入；固定帧长模式可留空"));
    form->addRow(tr("帧头"), m_headerEdit);

    m_lengthModeCombo = new QComboBox(frameGroup);
    m_lengthModeCombo->addItem(tr("长度字节"), static_cast<int>(FrameSpec::LengthMode::LengthByte));
    m_lengthModeCombo->addItem(tr("固定帧长"), static_cast<int>(FrameSpec::LengthMode::Fixed));
    form->addRow(tr("帧长模式"), m_lengthModeCombo);

    m_lengthOffsetSpin = new QSpinBox(frameGroup);
    m_lengthOffsetSpin->setRange(0, 512);
    m_lengthOffsetSpin->setValue(2);
    m_lengthOffsetSpin->setToolTip(tr("长度字节在帧内的偏移（相对帧首）"));
    form->addRow(tr("长度字节偏移"), m_lengthOffsetSpin);

    m_lengthAdjustSpin = new QSpinBox(frameGroup);
    m_lengthAdjustSpin->setRange(-1024, 1024);
    m_lengthAdjustSpin->setValue(6);
    m_lengthAdjustSpin->setToolTip(
        tr("帧长 = 长度字节值 + 补偿。\n"
           "例：长度值只含载荷，帧头 2B + 长度 1B + 命令 1B + CRC 2B，则补偿填 6。"));
    form->addRow(tr("长度补偿"), m_lengthAdjustSpin);

    m_fixedLengthSpin = new QSpinBox(frameGroup);
    m_fixedLengthSpin->setRange(1, 4096);
    m_fixedLengthSpin->setValue(16);
    form->addRow(tr("固定帧长"), m_fixedLengthSpin);

    m_checksumCombo = new QComboBox(frameGroup);
    m_checksumCombo->addItem(tr("不校验"), static_cast<int>(FrameChecksum::None));
    m_checksumCombo->addItem(tr("CRC16-Modbus（小端）"), static_cast<int>(FrameChecksum::Crc16Modbus));
    m_checksumCombo->addItem(tr("CRC16-CCITT（高字节在前）"), static_cast<int>(FrameChecksum::Crc16Ccitt));
    m_checksumCombo->addItem(tr("Sum8（累加和）"), static_cast<int>(FrameChecksum::Sum8));
    m_checksumCombo->addItem(tr("Xor8（异或）"), static_cast<int>(FrameChecksum::Xor8));
    m_checksumCombo->addItem(tr("LRC8（补码）"), static_cast<int>(FrameChecksum::Lrc8));
    m_checksumCombo->setToolTip(
        tr("帧内顺序为 [帧头][长度/数据][校验][帧尾]；\n"
           "选择校验算法后，长度补偿需包含校验与帧尾的字节数。"));
    form->addRow(tr("校验"), m_checksumCombo);

    m_tailEdit = new QLineEdit(frameGroup);
    m_tailEdit->setPlaceholderText(tr("帧尾，如 0D 0A（留空表示无帧尾）"));
    m_tailEdit->setToolTip(tr("帧尾固定字节序列，位于校验字段之后"));
    form->addRow(tr("帧尾"), m_tailEdit);

    m_endianCombo = new QComboBox(frameGroup);
    m_endianCombo->addItem(tr("小端（低字节在前）"), false);
    m_endianCombo->addItem(tr("大端（高字节在前）"), true);
    m_endianCombo->setToolTip(tr("字段数值的字节序；帧头 / 长度域不受此设置影响"));
    form->addRow(tr("字段字节序"), m_endianCombo);

    auto *fieldsGroup = new QGroupBox(tr("数据通道（偏移相对帧首）"), this);
    auto *fieldsLayout = new QVBoxLayout(fieldsGroup);

    m_fieldTable = new QTableWidget(0, 3, fieldsGroup);
    m_fieldTable->setHorizontalHeaderLabels({tr("名称"), tr("偏移"), tr("类型")});
    m_fieldTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fieldTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_fieldTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fieldTable->verticalHeader()->setVisible(false);
    m_fieldTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fieldTable->setMinimumHeight(180);
    fieldsLayout->addWidget(m_fieldTable);

    auto *fieldButtons = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("添加通道"), fieldsGroup);
    auto *removeButton = new QPushButton(tr("删除选中"), fieldsGroup);
    connect(addButton, &QPushButton::clicked, this, &HexFieldConfigDialog::addFieldRow);
    connect(removeButton, &QPushButton::clicked, this, &HexFieldConfigDialog::removeSelectedRow);
    fieldButtons->addWidget(addButton);
    fieldButtons->addWidget(removeButton);
    fieldButtons->addStretch(1);
    fieldsLayout->addLayout(fieldButtons);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setProperty("error", true);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &HexFieldConfigDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(frameGroup);
    root->addWidget(fieldsGroup, 1);
    root->addWidget(m_errorLabel);
    root->addWidget(buttons);

    connect(m_lengthModeCombo, &QComboBox::currentIndexChanged, this,
            &HexFieldConfigDialog::updateLengthWidgets);

    HexParseSpec defaults;
    defaults.fields = {HexField{QStringLiteral("CH1"), 3, FieldType::UInt16},
                       HexField{QStringLiteral("CH2"), 5, FieldType::UInt16}};
    setSpec(defaults);
}

void HexFieldConfigDialog::updateLengthWidgets()
{
    const auto mode = static_cast<FrameSpec::LengthMode>(
        m_lengthModeCombo->currentData().toInt());
    const bool lengthByte = mode == FrameSpec::LengthMode::LengthByte;
    m_lengthOffsetSpin->setEnabled(lengthByte);
    m_lengthAdjustSpin->setEnabled(lengthByte);
    m_fixedLengthSpin->setEnabled(!lengthByte);
    m_headerEdit->setPlaceholderText(lengthByte ? QStringLiteral("AA 55")
                                                : tr("可留空"));
}

void HexFieldConfigDialog::setFieldRow(int row, const HexField &field)
{
    auto *nameEdit = new QLineEdit(field.name, m_fieldTable);
    m_fieldTable->setCellWidget(row, 0, nameEdit);

    auto *offsetSpin = new QSpinBox(m_fieldTable);
    offsetSpin->setRange(0, m_maxFrameLength - 1);
    offsetSpin->setValue(qBound(0, field.offset, m_maxFrameLength - 1));
    m_fieldTable->setCellWidget(row, 1, offsetSpin);

    QComboBox *typeCombo = createTypeCombo(m_fieldTable);
    typeCombo->setCurrentText(HexFieldExtractor::fieldTypeName(field.type));
    m_fieldTable->setCellWidget(row, 2, typeCombo);
}

void HexFieldConfigDialog::addFieldRow()
{
    const int row = m_fieldTable->rowCount();
    m_fieldTable->insertRow(row);
    HexField field;
    field.name = tr("CH%1").arg(row + 1);
    field.offset = 3 + row * 2;
    field.type = FieldType::UInt16;
    setFieldRow(row, field);
    m_fieldTable->selectRow(row);
}

void HexFieldConfigDialog::removeSelectedRow()
{
    const int row = m_fieldTable->currentRow();
    if (row >= 0)
        m_fieldTable->removeRow(row);
}

void HexFieldConfigDialog::setSpec(const HexParseSpec &spec)
{
    m_maxFrameLength = qMax(8, spec.frame.maxFrameLength);
    m_headerEdit->setText(QString::fromLatin1(spec.frame.header.toHex(' ').toUpper()));
    m_lengthModeCombo->setCurrentIndex(
        m_lengthModeCombo->findData(static_cast<int>(spec.frame.lengthMode)));
    m_lengthOffsetSpin->setValue(spec.frame.lengthByteOffset);
    m_lengthAdjustSpin->setValue(spec.frame.lengthAdjust);
    m_fixedLengthSpin->setValue(spec.frame.fixedFrameLength);
    const int checksumIndex =
        m_checksumCombo->findData(static_cast<int>(spec.frame.checksum));
    m_checksumCombo->setCurrentIndex(checksumIndex >= 0 ? checksumIndex : 0);
    m_tailEdit->setText(
        QString::fromLatin1(spec.frame.tail.toHex(' ').toUpper()));
    m_endianCombo->setCurrentIndex(spec.bigEndian ? 1 : 0);
    updateLengthWidgets();

    m_fieldTable->setRowCount(0);
    for (const HexField &field : spec.fields) {
        const int row = m_fieldTable->rowCount();
        m_fieldTable->insertRow(row);
        setFieldRow(row, field);
    }
}

HexParseSpec HexFieldConfigDialog::readSpec() const
{
    HexParseSpec spec;
    QString headerError;
    spec.frame.header = HexCodec::parseHex(m_headerEdit->text(), &headerError);
    spec.frame.lengthMode = static_cast<FrameSpec::LengthMode>(
        m_lengthModeCombo->currentData().toInt());
    spec.frame.lengthByteOffset = m_lengthOffsetSpin->value();
    spec.frame.lengthAdjust = m_lengthAdjustSpin->value();
    spec.frame.fixedFrameLength = m_fixedLengthSpin->value();
    spec.frame.checksum = static_cast<FrameChecksum>(m_checksumCombo->currentData().toInt());
    QString tailError;
    spec.frame.tail = HexCodec::parseHex(m_tailEdit->text(), &tailError);
    spec.frame.tailMode = spec.frame.tail.isEmpty() ? FrameTailMode::None
                                                    : FrameTailMode::Fixed;
    spec.frame.maxFrameLength = m_maxFrameLength;
    spec.bigEndian = m_endianCombo->currentData().toBool();

    for (int row = 0; row < m_fieldTable->rowCount(); ++row) {
        HexField field;
        auto *nameEdit = qobject_cast<QLineEdit *>(m_fieldTable->cellWidget(row, 0));
        auto *offsetSpin = qobject_cast<QSpinBox *>(m_fieldTable->cellWidget(row, 1));
        auto *typeCombo = qobject_cast<QComboBox *>(m_fieldTable->cellWidget(row, 2));
        field.name = nameEdit ? nameEdit->text().trimmed() : QString();
        if (field.name.isEmpty())
            field.name = tr("CH%1").arg(row + 1);
        field.offset = offsetSpin ? offsetSpin->value() : 0;
        field.type = typeCombo
            ? HexFieldExtractor::fieldTypeFromName(typeCombo->currentText())
            : FieldType::UInt16;
        spec.fields.append(field);
    }
    return spec;
}

QString HexFieldConfigDialog::validationError() const
{
    QString headerError;
    const QByteArray header = HexCodec::parseHex(m_headerEdit->text(), &headerError);
    if (!headerError.isEmpty())
        return tr("帧头格式错误：%1").arg(headerError);

    const auto mode = static_cast<FrameSpec::LengthMode>(
        m_lengthModeCombo->currentData().toInt());
    if (mode == FrameSpec::LengthMode::LengthByte && header.isEmpty())
        return tr("长度字节模式需要填写帧头");

    if (m_fieldTable->rowCount() == 0)
        return tr("至少需要配置一个数据通道");

    const int frameLimit = (mode == FrameSpec::LengthMode::Fixed)
        ? m_fixedLengthSpin->value()
        : m_maxFrameLength;
    for (int row = 0; row < m_fieldTable->rowCount(); ++row) {
        auto *offsetSpin = qobject_cast<QSpinBox *>(m_fieldTable->cellWidget(row, 1));
        auto *typeCombo = qobject_cast<QComboBox *>(m_fieldTable->cellWidget(row, 2));
        if (!offsetSpin || !typeCombo)
            continue;
        const FieldType type = HexFieldExtractor::fieldTypeFromName(typeCombo->currentText());
        const int end = offsetSpin->value() + HexFieldExtractor::fieldSize(type);
        if (end > frameLimit) {
            return tr("第 %1 个通道超出帧长范围（偏移 %2 + 类型宽度 > %3）")
                .arg(row + 1)
                .arg(offsetSpin->value())
                .arg(frameLimit);
        }
    }

    if (mode == FrameSpec::LengthMode::Fixed && m_fixedLengthSpin->value() < header.size() + 1)
        return tr("固定帧长必须大于帧头长度");

    return {};
}

void HexFieldConfigDialog::onAccept()
{
    const QString error = validationError();
    if (!error.isEmpty()) {
        m_errorLabel->setText(error);
        m_errorLabel->show();
        return;
    }
    m_errorLabel->hide();
    accept();
}

HexParseSpec HexFieldConfigDialog::spec() const
{
    return readSpec();
}
