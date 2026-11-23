#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QPlainTextEdit;
class QTableWidget;

/// 编码转换与乱码修复工具
///
/// 两种使用方式：
/// 1. 粘贴乱码文本 → 选择「原始编码」和「目标编码」→ 转换
/// 2. 粘贴乱码对应的 HEX 原始字节 → 逐个编码试解，按可读性评分排序后挑选
class EncodingFixDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EncodingFixDialog(QWidget *parent = nullptr);

private slots:
    void onModeChanged();
    void updatePreview();
    void copyResult();

private:
    void setupUi();
    /// 刷新「试解候选」表格
    void refreshCandidates();
    /// 采集原始字节：HEX 模式取解析结果，文本模式取按原始编码编码后的字节
    QByteArray collectBytes(bool *ok) const;
    /// 当前是否为 HEX 输入模式
    bool isHexMode() const;

    bool m_hexMode = false;
    QComboBox *m_sourceCombo = nullptr;
    QComboBox *m_targetCombo = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QPlainTextEdit *m_output = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_candidateTable = nullptr;
};
