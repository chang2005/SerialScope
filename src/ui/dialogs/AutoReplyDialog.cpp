#include "ui/dialogs/AutoReplyDialog.h"

#include "ui/Theme.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

AutoReplyDialog::AutoReplyDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    reloadTable();
}

void AutoReplyDialog::setupUi()
{
    setWindowTitle(tr("自动回复规则"));
    resize(720, 420);

    auto *layout = new QVBoxLayout(this);

    auto *hint = new QLabel(
        tr("当收到的数据匹配规则时，自动回复预设内容，用于模拟从机设备。"
           "回复内容支持 \\r \\n \\t \\\\ \\xNN 转义。"),
        this);
    hint->setWordWrap(true);
    hint->setProperty("muted", true);
    layout->addWidget(hint);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {tr("名称"), tr("匹配内容"), tr("回复内容"), tr("正则"), tr("启用")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                             | QAbstractItemView::SelectedClicked
                             | QAbstractItemView::EditKeyPressed);
    layout->addWidget(m_table, 1);

    auto *buttonRow = new QHBoxLayout();
    m_addButton = new QPushButton(tr("添加规则"), this);
    buttonRow->addWidget(m_addButton);
    m_removeButton = new QPushButton(tr("删除选中"), this);
    buttonRow->addWidget(m_removeButton);

    m_enabledCheck = new QCheckBox(tr("启用自动回复"), this);
    m_enabledCheck->setChecked(true);
    m_enabledCheck->setToolTip(tr("关闭后所有规则都不生效"));
    buttonRow->addWidget(m_enabledCheck);

    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         this);
    layout->addWidget(buttons);

    connect(m_addButton, &QPushButton::clicked, this, &AutoReplyDialog::onAddClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &AutoReplyDialog::onRemoveClicked);
    connect(m_table, &QTableWidget::cellChanged, this, &AutoReplyDialog::onCellChanged);
    connect(m_enabledCheck, &QCheckBox::toggled, this, &AutoReplyDialog::onEnabledToggled);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AutoReplyDialog::setRules(const AutoReplyEngine &rules)
{
    m_rules = rules;
    reloadTable();
}

void AutoReplyDialog::reloadTable()
{
    const bool blocked = m_table->blockSignals(true);
    m_table->setRowCount(0);

    const QVector<AutoReplyRule> &rules = m_rules.rules();
    m_table->setRowCount(rules.size());
    for (int row = 0; row < rules.size(); ++row) {
        const AutoReplyRule &rule = rules.at(row);

        auto *nameItem = new QTableWidgetItem(rule.name);
        nameItem->setFlags(nameItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(row, 0, nameItem);

        auto *matchItem = new QTableWidgetItem(rule.matchPattern);
        matchItem->setFlags(matchItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(row, 1, matchItem);

        auto *replyItem = new QTableWidgetItem(rule.replyPayload);
        replyItem->setFlags(replyItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(row, 2, replyItem);

        auto *regexItem = new QTableWidgetItem(rule.useRegex ? tr("是") : tr("否"));
        regexItem->setFlags(regexItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(row, 3, regexItem);

        auto *enabledItem = new QTableWidgetItem(rule.enabled ? tr("是") : tr("否"));
        enabledItem->setFlags(enabledItem->flags() | Qt::ItemIsEditable);
        m_table->setItem(row, 4, enabledItem);
    }
    m_table->blockSignals(blocked);
}

void AutoReplyDialog::applyRowToRule(int row)
{
    if (row < 0 || row >= m_rules.count())
        return;

    AutoReplyRule rule = m_rules.rules().at(row);
    if (QTableWidgetItem *item = m_table->item(row, 0))
        rule.name = item->text().trimmed();
    if (QTableWidgetItem *item = m_table->item(row, 1))
        rule.matchPattern = item->text();
    if (QTableWidgetItem *item = m_table->item(row, 2))
        rule.replyPayload = item->text();
    if (QTableWidgetItem *item = m_table->item(row, 3))
        rule.useRegex = (item->text().trimmed() == tr("是"));
    if (QTableWidgetItem *item = m_table->item(row, 4))
        rule.enabled = (item->text().trimmed() == tr("否") ? false : true);

    if (rule.matchPattern.isEmpty() || rule.replyPayload.isEmpty())
        return;

    m_rules.updateRule(m_rules.rules().at(row).name, rule);
    emit rulesChanged(m_rules);
}

void AutoReplyDialog::onCellChanged(int row, int column)
{
    Q_UNUSED(column);
    applyRowToRule(row);
}

void AutoReplyDialog::onEnabledToggled()
{
    // 逐条规则的启用状态由表格控制，这里的总开关作用于所有规则
    const bool global = m_enabledCheck->isChecked();
    QVector<AutoReplyRule> rules = m_rules.rules();
    for (AutoReplyRule &rule : rules)
        rule.enabled = global;
    m_rules = AutoReplyEngine();
    for (const AutoReplyRule &rule : rules)
        m_rules.addRule(rule);
    reloadTable();
    emit rulesChanged(m_rules);
}

void AutoReplyDialog::onAddClicked()
{
    AutoReplyRule rule;
    rule.name = tr("规则 %1").arg(m_rules.count() + 1);
    rule.matchPattern = QStringLiteral("PING");
    rule.replyPayload = QStringLiteral("PONG");
    if (!m_rules.addRule(rule))
        return;
    reloadTable();
    emit rulesChanged(m_rules);
}

void AutoReplyDialog::onRemoveClicked()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_rules.count())
        return;

    m_rules.removeRule(m_rules.rules().at(row).name);
    reloadTable();
    emit rulesChanged(m_rules);
}
