#pragma once

#include "core/parser/HexFieldExtractor.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

/// HEX 波形数据源协议配置：帧头、长度模式、CRC、字节序与字段表
class HexFieldConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HexFieldConfigDialog(QWidget *parent = nullptr);

    void setSpec(const HexParseSpec &spec);
    HexParseSpec spec() const;

private slots:
    void addFieldRow();
    void removeSelectedRow();
    void updateLengthWidgets();
    void onAccept();

private:
    void setupUi();
    HexParseSpec readSpec() const;
    void setFieldRow(int row, const HexField &field);
    QString validationError() const;

    QLineEdit *m_headerEdit = nullptr;
    QComboBox *m_lengthModeCombo = nullptr;
    QSpinBox *m_lengthOffsetSpin = nullptr;
    QSpinBox *m_lengthAdjustSpin = nullptr;
    QSpinBox *m_fixedLengthSpin = nullptr;
    QComboBox *m_checksumCombo = nullptr;
    QLineEdit *m_tailEdit = nullptr;
    QComboBox *m_endianCombo = nullptr;
    QTableWidget *m_fieldTable = nullptr;
    QLabel *m_errorLabel = nullptr;
    int m_maxFrameLength = 4096;
};
