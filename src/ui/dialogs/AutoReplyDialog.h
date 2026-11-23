#pragma once

#include "core/AutoReply.h"

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QTableWidget;
class QPushButton;

/// 自动回复规则编辑对话框。
///
/// 用于模拟从机设备：收到匹配的请求后自动回复预设内容，
/// 让主机程序在没有真实硬件时也能联调。
class AutoReplyDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AutoReplyDialog(QWidget *parent = nullptr);

    /// 取回编辑后的规则集合
    AutoReplyEngine rules() const { return m_rules; }
    /// 用给定规则初始化对话框
    void setRules(const AutoReplyEngine &rules);

signals:
    /// 规则被修改（对话框内每次增删改都会发出）
    void rulesChanged(const AutoReplyEngine &rules);

private slots:
    void onAddClicked();
    void onRemoveClicked();
    void onCellChanged(int row, int column);
    void onEnabledToggled();

private:
    void setupUi();
    void reloadTable();
    void showRuleDialog(int row);
    void applyRowToRule(int row);

    QTableWidget *m_table = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    AutoReplyEngine m_rules;
};
