#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

/// CRC / 校验和计算工具箱
class CrcToolDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CrcToolDialog(QWidget *parent = nullptr);

private slots:
    void updateResult();
    void copyResult();

private:
    void setupUi();

    QPlainTextEdit *m_input = nullptr;
    QComboBox *m_algorithmCombo = nullptr;
    QLineEdit *m_resultEdit = nullptr;
    QLabel *m_detailLabel = nullptr;
    QLabel *m_errorLabel = nullptr;
};
