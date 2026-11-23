#include "ui/dialogs/SequenceDialog.h"

#include "core/parser/HexCodec.h"

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

SequenceDialog::SequenceDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    reloadGroups();
    updateButtons();
}

void SequenceDialog::setupUi()
{
    setWindowTitle(tr("轮发序列编辑器"));
    resize(880, 520);

    auto *layout = new QVBoxLayout(this);

    auto *hint = new QLabel(
        tr("轮发序列由若干「组」组成，每组内有多条数据。"
           "轮发时按组顺序、组内顺序逐条发送，全部组发完自动回到第一组循环。"),
        this);
    hint->setWordWrap(true);
    hint->setProperty("muted", true);
    layout->addWidget(hint);

    auto *body = new QHBoxLayout();

    // ---- 左：组列表 ----
    auto *groupBox = new QGroupBox(tr("组"), this);
    auto *groupLayout = new QVBoxLayout(groupBox);
    m_groupTable = new QTableWidget(0, 2, groupBox);
    m_groupTable->setHorizontalHeaderLabels({tr("组名"), tr("条目数")});
    m_groupTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_groupTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_groupTable->verticalHeader()->setVisible(false);
    m_groupTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_groupTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_groupTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    groupLayout->addWidget(m_groupTable, 1);

    auto *groupButtonRow = new QHBoxLayout();
    auto *addGroupButton = new QPushButton(tr("新建组"), groupBox);
    auto *removeGroupButton = new QPushButton(tr("删除组"), groupBox);
    auto *renameGroupButton = new QPushButton(tr("重命名"), groupBox);
    groupButtonRow->addWidget(addGroupButton);
    groupButtonRow->addWidget(removeGroupButton);
    groupButtonRow->addWidget(renameGroupButton);
    groupLayout->addLayout(groupButtonRow);

    body->addWidget(groupBox, 1);

    // ---- 右：条目列表 ----
    auto *itemBox = new QGroupBox(tr("当前组的条目"), this);
    auto *itemLayout = new QVBoxLayout(itemBox);

    auto *groupForm = new QFormLayout();
    m_groupNameEdit = new QLineEdit(itemBox);
    m_groupNameEdit->setPlaceholderText(tr("组名，如「查询组」"));
    groupForm->addRow(tr("组名"), m_groupNameEdit);
    m_intervalSpin = new QSpinBox(itemBox);
    m_intervalSpin->setRange(10, 3600000);
    m_intervalSpin->setSingleStep(100);
    m_intervalSpin->setValue(1000);
    m_intervalSpin->setSuffix(tr(" ms"));
    m_intervalSpin->setToolTip(tr("定时轮发时本组之间的间隔"));
    groupForm->addRow(tr("组间隔"), m_intervalSpin);
    itemLayout->addLayout(groupForm);

    m_itemTable = new QTableWidget(0, 2, itemBox);
    m_itemTable->setHorizontalHeaderLabels({tr("名称"), tr("内容")});
    m_itemTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_itemTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_itemTable->verticalHeader()->setVisible(false);
    m_itemTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_itemTable->setSelectionMode(QAbstractItemView::SingleSelection);
    itemLayout->addWidget(m_itemTable, 1);

    auto *itemForm = new QFormLayout();
    m_itemLabelEdit = new QLineEdit(itemBox);
    m_itemLabelEdit->setPlaceholderText(tr("条目名称（可留空）"));
    itemForm->addRow(tr("条目名"), m_itemLabelEdit);

    auto *payloadRow = new QWidget(itemBox);
    auto *payloadLayout = new QHBoxLayout(payloadRow);
    payloadLayout->setContentsMargins(0, 0, 0, 0);
    m_itemPayloadEdit = new QLineEdit(payloadRow);
    m_itemPayloadEdit->setPlaceholderText(tr("HEX 字节，如 01 03 00 00"));
    payloadLayout->addWidget(m_itemPayloadEdit, 1);
    itemForm->addRow(tr("内容 (HEX)"), payloadRow);
    itemLayout->addLayout(itemForm);

    auto *itemButtonRow = new QHBoxLayout();
    auto *addItemButton = new QPushButton(tr("添加条目"), itemBox);
    auto *removeItemButton = new QPushButton(tr("删除条目"), itemBox);
    itemButtonRow->addWidget(addItemButton);
    itemButtonRow->addWidget(removeItemButton);
    itemButtonRow->addStretch(1);
    itemLayout->addLayout(itemButtonRow);

    m_statusLabel = new QLabel(itemBox);
    m_statusLabel->setProperty("muted", true);
    m_statusLabel->setWordWrap(true);
    itemLayout->addWidget(m_statusLabel);

    body->addWidget(itemBox, 2);
    layout->addLayout(body, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         this);
    layout->addWidget(buttons);

    connect(addGroupButton, &QPushButton::clicked, this, &SequenceDialog::onAddGroup);
    connect(removeGroupButton, &QPushButton::clicked, this, &SequenceDialog::onRemoveGroup);
    connect(renameGroupButton, &QPushButton::clicked, this, &SequenceDialog::onRenameGroup);
    connect(m_groupTable, &QTableWidget::itemSelectionChanged, this, [this] {
        onGroupSelected(m_groupTable->currentRow());
    });
    connect(addItemButton, &QPushButton::clicked, this, &SequenceDialog::onAddItem);
    connect(removeItemButton, &QPushButton::clicked, this, &SequenceDialog::onRemoveItem);
    connect(m_itemTable, &QTableWidget::itemSelectionChanged, this, [this] {
        onItemSelected(m_itemTable->currentRow(), 0);
    });
    connect(m_itemTable, &QTableWidget::cellChanged, this, [this](int, int) {
        onItemEdited();
    });
    connect(m_itemLabelEdit, &QLineEdit::editingFinished, this,
            &SequenceDialog::onItemEdited);
    connect(m_itemPayloadEdit, &QLineEdit::editingFinished, this,
            &SequenceDialog::onItemEdited);
    connect(m_groupNameEdit, &QLineEdit::editingFinished, this, [this] {
        commitCurrentItem();
    });
    connect(m_intervalSpin, &QSpinBox::valueChanged, this, [this] {
        commitCurrentItem();
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SequenceDialog::setSequence(const SequenceSet &set)
{
    m_set = set;
    reloadGroups();
    if (m_set.count() > 0)
        onGroupSelected(0);
    updateButtons();
}

void SequenceDialog::reloadGroups()
{
    m_groupTable->setRowCount(m_set.count());
    for (int row = 0; row < m_set.count(); ++row) {
        const SequenceGroup *group = m_set.groupAt(row);
        if (!group)
            continue;
        m_groupTable->setItem(row, 0, new QTableWidgetItem(group->name));
        m_groupTable->setItem(row, 1,
                              new QTableWidgetItem(QString::number(group->items.size())));
    }
    if (m_currentGroup >= m_set.count())
        m_currentGroup = m_set.count() - 1;
}

void SequenceDialog::reloadItems()
{
    m_itemTable->blockSignals(true);
    m_itemTable->setRowCount(m_editingGroup.items.size());
    for (int row = 0; row < m_editingGroup.items.size(); ++row) {
        const SequenceItem &item = m_editingGroup.items.at(row);
        m_itemTable->setItem(row, 0, new QTableWidgetItem(item.label));
        m_itemTable->setItem(row, 1,
                             new QTableWidgetItem(QString::fromLatin1(item.payload.toHex(' '))));
    }
    m_itemTable->blockSignals(false);
    m_currentItem = -1;
    m_itemLabelEdit->clear();
    m_itemPayloadEdit->clear();
}

void SequenceDialog::commitCurrentItem()
{
    if (m_currentGroup < 0 || m_currentGroup >= m_set.count())
        return;

    // 组名与间隔的修改直接写回当前组
    SequenceGroup group = m_editingGroup;
    group.name = m_groupNameEdit->text().trimmed();
    if (group.name.isEmpty())
        group.name = QObject::tr("组 %1").arg(m_currentGroup + 1);
    group.intervalMs = m_intervalSpin->value();
    m_set.updateGroup(m_currentGroup, group);
    m_editingGroup = group;
    reloadGroups();
}

void SequenceDialog::onAddGroup()
{
    SequenceGroup group;
    group.name = QObject::tr("组 %1").arg(m_set.count() + 1);
    group.intervalMs = m_intervalSpin->value();
    SequenceItem item;
    item.label = tr("指令 1");
    item.payload = QByteArrayLiteral("01");
    group.items.append(item);

    const int index = m_set.addGroup(group);
    if (index < 0)
        return;
    m_currentGroup = index;
    m_editingGroup = group;
    reloadGroups();
    m_groupTable->selectRow(index);
    onGroupSelected(index);
    emit sequenceChanged();
}

void SequenceDialog::onRemoveGroup()
{
    if (m_currentGroup < 0 || m_currentGroup >= m_set.count())
        return;
    if (QMessageBox::question(this, tr("删除组"),
                              tr("确定删除当前组及其全部条目吗？"))
        != QMessageBox::Yes) {
        return;
    }
    m_set.removeGroup(m_currentGroup);
    m_currentGroup = -1;
    m_editingGroup = SequenceGroup();
    reloadGroups();
    reloadItems();
    if (m_set.count() > 0) {
        m_currentGroup = 0;
        const SequenceGroup *group = m_set.groupAt(0);
        if (group) {
            m_editingGroup = *group;
            m_groupNameEdit->setText(group->name);
            m_intervalSpin->setValue(group->intervalMs);
            reloadItems();
        }
    }
    emit sequenceChanged();
    updateButtons();
}

void SequenceDialog::onRenameGroup()
{
    commitCurrentItem();
    reloadGroups();
    emit sequenceChanged();
}

void SequenceDialog::onGroupSelected(int row)
{
    if (row < 0 || row >= m_set.count())
        return;
    m_currentGroup = row;
    const SequenceGroup *group = m_set.groupAt(row);
    if (!group)
        return;
    m_editingGroup = *group;
    m_groupNameEdit->setText(group->name);
    m_intervalSpin->setValue(group->intervalMs);
    reloadItems();
    updateButtons();
}

void SequenceDialog::onAddItem()
{
    if (m_currentGroup < 0 || m_currentGroup >= m_set.count())
        return;

    SequenceItem item;
    item.label = tr("指令 %1").arg(m_editingGroup.items.size() + 1);
    item.payload = QByteArrayLiteral("01");
    m_editingGroup.items.append(item);
    m_set.updateGroup(m_currentGroup, m_editingGroup);
    reloadItems();
    reloadGroups();
    emit sequenceChanged();
}

void SequenceDialog::onRemoveItem()
{
    if (m_currentGroup < 0 || m_currentGroup >= m_set.count())
        return;
    if (m_currentItem < 0 || m_currentItem >= m_editingGroup.items.size())
        return;

    m_editingGroup.items.remove(m_currentItem);
    m_set.updateGroup(m_currentGroup, m_editingGroup);
    reloadItems();
    reloadGroups();
    emit sequenceChanged();
}

void SequenceDialog::onItemSelected(int row, int column)
{
    Q_UNUSED(column)
    if (m_currentGroup < 0 || m_currentGroup >= m_set.count())
        return;
    if (row < 0 || row >= m_editingGroup.items.size())
        return;
    m_currentItem = row;
    const SequenceItem &item = m_editingGroup.items.at(row);
    m_itemLabelEdit->setText(item.label);
    m_itemPayloadEdit->setText(QString::fromLatin1(item.payload.toHex(' ')));
    m_statusLabel->setText(tr("HEX 内容；编辑后按回车或点击空白处生效"));
}

void SequenceDialog::onItemEdited()
{
    if (m_currentGroup < 0 || m_currentGroup >= m_set.count())
        return;
    if (m_currentItem < 0 || m_currentItem >= m_editingGroup.items.size())
        return;

    QString hexError;
    const QByteArray payload = HexCodec::parseHex(m_itemPayloadEdit->text(), &hexError);
    if (!hexError.isEmpty()) {
        m_statusLabel->setText(tr("HEX 无效：%1").arg(hexError));
        m_statusLabel->setProperty("error", true);
        return;
    }
    m_statusLabel->setProperty("error", false);

    m_editingGroup.items[m_currentItem].label = m_itemLabelEdit->text();
    m_editingGroup.items[m_currentItem].payload = payload;
    m_set.updateGroup(m_currentGroup, m_editingGroup);
    reloadItems();
    reloadGroups();
    emit sequenceChanged();
}

void SequenceDialog::updateButtons()
{
    const bool hasGroup = m_currentGroup >= 0 && m_currentGroup < m_set.count();
    const bool hasItem = hasGroup && m_currentItem >= 0
                         && m_currentItem < m_editingGroup.items.size();
    m_itemLabelEdit->setEnabled(hasItem);
    m_itemPayloadEdit->setEnabled(hasItem);
}
