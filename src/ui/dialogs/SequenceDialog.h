#pragma once

#include "core/SequenceSender.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

/// 轮发序列编辑器：管理多组数据、每组多条目的轮流发送
///
/// 界面分三块：
/// - 左侧：组列表（可增删改）
/// - 右侧：当前组的条目列表（可增删改）
/// - 底部：编码与 HEX 开关，条目在此模式下编辑
class SequenceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SequenceDialog(QWidget *parent = nullptr);

    /// 取当前编辑好的组集合
    const SequenceSet &sequence() const { return m_set; }
    /// 预置组集合
    void setSequence(const SequenceSet &set);

signals:
    /// 组集合发生变化（每次增删改后发出）
    void sequenceChanged();

private slots:
    void onAddGroup();
    void onRemoveGroup();
    void onRenameGroup();
    void onGroupSelected(int row);
    void onAddItem();
    void onRemoveItem();
    void onItemSelected(int row, int column);
    void onItemEdited();
    void updateButtons();

private:
    void setupUi();
    void reloadGroups();
    void reloadItems();
    void commitCurrentItem();

    SequenceSet m_set;
    SequenceGroup m_editingGroup; ///< 正在编辑的组（尚未写回 m_set）
    int m_currentGroup = -1;
    int m_currentItem = -1;

    QTableWidget *m_groupTable = nullptr;
    QTableWidget *m_itemTable = nullptr;
    QLineEdit *m_groupNameEdit = nullptr;
    QSpinBox *m_intervalSpin = nullptr;
    QLineEdit *m_itemLabelEdit = nullptr;
    QLineEdit *m_itemPayloadEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
};
