#include "ui/SerialConfigPanel.h"

#include "ui/FlowLayout.h"
#include "ui/WidgetUtil.h"
#include "utils/SettingsManager.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCollator>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHash>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSet>
#include <QSettings>
#include <QStyle>
#include <QToolButton>

#include <algorithm>

namespace {

QLabel *makeLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("muted", true);
    return label;
}

void repolish(QWidget *widget)
{
    WidgetUtil::repolish(widget);
}

QString normalizePortName(const QString &rawName)
{
    QString name = rawName.trimmed();
#ifdef Q_OS_WIN
    // 也接受 \\.\COM3 这种 Windows 原始设备路径。
    if (name.startsWith(QStringLiteral("\\\\.\\")))
        name = name.mid(4);
    // COM 端口名不区分大小写，统一成 COMx，便于比较和保存。
    if (name.size() > 3 && name.left(3).compare(QStringLiteral("com"), Qt::CaseInsensitive) == 0) {
        bool digitsOnly = true;
        for (int i = 3; i < name.size(); ++i) {
            if (!name.at(i).isDigit()) {
                digitsOnly = false;
                break;
            }
        }
        if (digitsOnly)
            name = QStringLiteral("COM") + name.mid(3);
    }
#else
    // QSerialPortInfo 返回 ttyUSB0 这样的短名；也接受用户输入 /dev/ttyUSB0。
    // by-id 这类多级路径保留完整形式，避免改成无法打开的相对路径。
    if (name.startsWith(QStringLiteral("/dev/")) && !name.mid(5).contains(QLatin1Char('/')))
        name = name.mid(5);
#endif
    return name;
}

bool samePortName(const QString &a, const QString &b)
{
#ifdef Q_OS_WIN
    return normalizePortName(a).compare(normalizePortName(b), Qt::CaseInsensitive) == 0;
#else
    return normalizePortName(a) == normalizePortName(b);
#endif
}

bool containsPort(const QStringList &names, const QString &name)
{
    return std::any_of(names.cbegin(), names.cend(),
                       [&name](const QString &candidate) { return samePortName(candidate, name); });
}

QString portToolTip(const QSerialPortInfo &info)
{
    QStringList details;
    if (!info.description().isEmpty())
        details << info.description();
    if (!info.manufacturer().isEmpty())
        details << QObject::tr("制造商：%1").arg(info.manufacturer());
    if (!info.serialNumber().isEmpty())
        details << QObject::tr("序列号：%1").arg(info.serialNumber());
    if (!info.systemLocation().isEmpty())
        details << QObject::tr("系统位置：%1").arg(info.systemLocation());
    details << QObject::tr("端口：%1").arg(info.portName());
    return details.join(QLatin1Char('\n'));
}

QString portSnapshotEntry(const QSerialPortInfo &info)
{
    return QStringList{info.portName(), info.description(), info.manufacturer(),
                       info.serialNumber(), info.systemLocation()}
        .join(QChar(0x1f));
}

const QList<int> &commonBaudRates()
{
    static const QList<int> rates = {
        300,    600,    1200,   2400,   4800,   9600,   14400,  19200,
        28800,  38400,  57600,  76800,  115200, 128000, 230400, 256000,
        460800, 500000, 576000, 921600, 1000000, 1152000, 1500000, 2000000,
        2500000, 3000000, 3500000, 4000000
    };
    return rates;
}

} // namespace

SerialConfigPanel::SerialConfigPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connect(m_openButton, &QPushButton::clicked, this, &SerialConfigPanel::onOpenClicked);
    connect(m_refreshButton, &QToolButton::clicked, this, &SerialConfigPanel::refreshPorts);

    // 先恢复配置，再挂保存连接，避免恢复过程覆盖已保存设置
    restoreSettings();
    refreshPorts();

    connect(m_portCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_portUserEdited = true;
        updateOpenButtonState();
        updatePortToolTip();
    });
    // 可编辑下拉框输入文字时 currentIndex 不一定变化，需要单独刷新按钮与提示。
    connect(m_portCombo, &QComboBox::currentTextChanged, this, [this] {
        m_portUserEdited = true;
        updateOpenButtonState();
        updatePortToolTip();
    });
    connect(m_portCombo, QOverload<int>::of(&QComboBox::activated),
            this, &SerialConfigPanel::saveSettings);
    if (QLineEdit *edit = m_portCombo->lineEdit()) {
        connect(edit, &QLineEdit::editingFinished, this, &SerialConfigPanel::saveSettings);
    }

    // 波特率只在选择完成或输入结束后保存，避免把 2、25 这类中间输入写进设置。
    connect(m_baudCombo, &QComboBox::currentTextChanged, this, [this] {
        updateOpenButtonState();
    });
    connect(m_baudCombo, QOverload<int>::of(&QComboBox::activated),
            this, &SerialConfigPanel::commitBaud);
    if (QLineEdit *edit = m_baudCombo->lineEdit())
        connect(edit, &QLineEdit::editingFinished, this, &SerialConfigPanel::commitBaud);

    connect(m_dataBitsCombo, &QComboBox::currentIndexChanged, this,
            &SerialConfigPanel::saveSettings);
    connect(m_parityCombo, &QComboBox::currentIndexChanged, this,
            &SerialConfigPanel::saveSettings);
    connect(m_stopBitsCombo, &QComboBox::currentIndexChanged, this,
            &SerialConfigPanel::saveSettings);
    connect(m_flowCombo, &QComboBox::currentIndexChanged, this,
            &SerialConfigPanel::saveSettings);

    connect(m_dtrButton, &QToolButton::toggled, this, [this](bool on) {
        emit dtrChanged(on);
        saveSettings();
    });
    connect(m_rtsButton, &QToolButton::toggled, this, [this](bool on) {
        emit rtsChanged(on);
        saveSettings();
    });
    connect(m_autoReconnectCheck, &QCheckBox::toggled, this, [this](bool on) {
        emit autoReconnectChanged(on);
        saveSettings();
    });

    updateOpenButtonState();
    updatePortToolTip();
}

SerialConfigPanel::~SerialConfigPanel()
{
    saveSettings();
}

void SerialConfigPanel::setupUi()
{
    // 用 FlowLayout 让控件在窗口变窄时自动换行，避免把整行控件挤扁
    auto *layout = new FlowLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);

    layout->addWidget(makeLabel(tr("串口"), this));

    m_portCombo = new QComboBox(this);
    m_portCombo->setObjectName(QStringLiteral("portCombo"));
    m_portCombo->setEditable(true);
    m_portCombo->setInsertPolicy(QComboBox::NoInsert);
    m_portCombo->setMinimumWidth(190);
    m_portCombo->setMaximumWidth(320);
    m_portCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_portCombo->setMaxVisibleItems(16);
    if (QLineEdit *edit = m_portCombo->lineEdit()) {
        edit->setPlaceholderText(tr("选择或输入端口，如 COM3"));
        edit->setClearButtonEnabled(true);
    }
    layout->addWidget(m_portCombo);

    m_refreshButton = new QToolButton(this);
    m_refreshButton->setObjectName(QStringLiteral("refreshButton"));
    m_refreshButton->setText(tr("刷新"));
    m_refreshButton->setToolTip(
        tr("重新枚举串口（每 2 秒自动刷新；已保存但未插入的端口会保留）"));
    layout->addWidget(m_refreshButton);

    layout->addWidget(makeLabel(tr("波特率"), this));
    m_baudCombo = new QComboBox(this);
    m_baudCombo->setObjectName(QStringLiteral("baudCombo"));
    m_baudCombo->setEditable(true);
    m_baudCombo->setInsertPolicy(QComboBox::NoInsert);
    m_baudCombo->setMinimumWidth(100);
    for (int rate : commonBaudRates())
        m_baudCombo->addItem(QString::number(rate), rate);
    m_baudCombo->setCurrentText(QStringLiteral("115200"));
    m_baudCombo->setToolTip(tr("常用波特率可直接选择，也可输入 50–20000000 之间的自定义值"));
    if (QLineEdit *edit = m_baudCombo->lineEdit()) {
        edit->setValidator(new QIntValidator(50, 20000000, m_baudCombo));
        edit->setPlaceholderText(QStringLiteral("115200"));
    }
    layout->addWidget(m_baudCombo);

    layout->addWidget(makeLabel(tr("数据位"), this));
    m_dataBitsCombo = new QComboBox(this);
    m_dataBitsCombo->setObjectName(QStringLiteral("dataBitsCombo"));
    m_dataBitsCombo->addItem(QStringLiteral("5"), QSerialPort::Data5);
    m_dataBitsCombo->addItem(QStringLiteral("6"), QSerialPort::Data6);
    m_dataBitsCombo->addItem(QStringLiteral("7"), QSerialPort::Data7);
    m_dataBitsCombo->addItem(QStringLiteral("8"), QSerialPort::Data8);
    m_dataBitsCombo->setCurrentIndex(3);
    m_dataBitsCombo->setToolTip(tr("每帧数据位数，绝大多数设备使用 8 位"));
    layout->addWidget(m_dataBitsCombo);

    layout->addWidget(makeLabel(tr("校验位"), this));
    m_parityCombo = new QComboBox(this);
    m_parityCombo->setObjectName(QStringLiteral("parityCombo"));
    m_parityCombo->addItem(tr("无校验"), QSerialPort::NoParity);
    m_parityCombo->addItem(tr("奇校验"), QSerialPort::OddParity);
    m_parityCombo->addItem(tr("偶校验"), QSerialPort::EvenParity);
    m_parityCombo->addItem(tr("标记 (Mark)"), QSerialPort::MarkParity);
    m_parityCombo->addItem(tr("空格 (Space)"), QSerialPort::SpaceParity);
    m_parityCombo->setToolTip(tr("无/奇/偶校验最常用；标记/空格校验需要驱动支持"));
    layout->addWidget(m_parityCombo);

    layout->addWidget(makeLabel(tr("停止位"), this));
    m_stopBitsCombo = new QComboBox(this);
    m_stopBitsCombo->setObjectName(QStringLiteral("stopBitsCombo"));
    m_stopBitsCombo->addItem(QStringLiteral("1"), QSerialPort::OneStop);
    m_stopBitsCombo->addItem(QStringLiteral("1.5"), QSerialPort::OneAndHalfStop);
    m_stopBitsCombo->addItem(QStringLiteral("2"), QSerialPort::TwoStop);
    m_stopBitsCombo->setToolTip(
        tr("1 位最常用；1.5 位仅 Windows 支持，并且需要配合 5 位数据位"));
    layout->addWidget(m_stopBitsCombo);

    layout->addWidget(makeLabel(tr("流控"), this));
    m_flowCombo = new QComboBox(this);
    m_flowCombo->setObjectName(QStringLiteral("flowCombo"));
    m_flowCombo->addItem(tr("无"), QSerialPort::NoFlowControl);
    m_flowCombo->addItem(tr("硬件"), QSerialPort::HardwareControl);
    m_flowCombo->addItem(tr("软件"), QSerialPort::SoftwareControl);
    m_flowCombo->setToolTip(tr("无流控最常用；硬件为 RTS/CTS，软件为 XON/XOFF"));
    layout->addWidget(m_flowCombo);

    // ---- 控制线：DTR / RTS 手动置位与复位 ----
    m_dtrButton = new QToolButton(this);
    m_dtrButton->setObjectName(QStringLiteral("dtrButton"));
    m_dtrButton->setText(tr("DTR"));
    m_dtrButton->setCheckable(true);
    m_dtrButton->setToolTip(
        tr("置位 / 复位 DTR（数据终端就绪）。常用于复位设备或进入下载模式"));
    layout->addWidget(m_dtrButton);

    m_rtsButton = new QToolButton(this);
    m_rtsButton->setObjectName(QStringLiteral("rtsButton"));
    m_rtsButton->setText(tr("RTS"));
    m_rtsButton->setCheckable(true);
    m_rtsButton->setToolTip(
        tr("置位 / 复位 RTS（请求发送）。常用于复位设备或切换收发方向"));
    layout->addWidget(m_rtsButton);

    // ---- 自动重连 ----
    m_autoReconnectCheck = new QCheckBox(tr("自动重连"), this);
    m_autoReconnectCheck->setObjectName(QStringLiteral("autoReconnectCheck"));
    m_autoReconnectCheck->setToolTip(
        tr("串口意外断开或设备复位后，自动按相同参数重新打开（间隔 2 秒）"));
    layout->addWidget(m_autoReconnectCheck);

    m_openButton = new QPushButton(tr("打开串口"), this);
    m_openButton->setObjectName(QStringLiteral("openButton"));
    m_openButton->setMinimumWidth(96);
    m_openButton->setProperty("primary", true);
    layout->addWidget(m_openButton);
}

void SerialConfigPanel::refreshPorts()
{
    if (m_connected || m_openPending)
        return;

    auto ports = QSerialPortInfo::availablePorts();
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(ports.begin(), ports.end(),
              [&collator](const QSerialPortInfo &a, const QSerialPortInfo &b) {
                  return collator.compare(a.portName(), b.portName()) < 0;
              });

    QHash<QString, int> descriptionCounts;
    for (const QSerialPortInfo &info : ports) {
        if (!info.description().isEmpty())
            ++descriptionCounts[info.description()];
    }

    QStringList snapshot;
    QStringList names;
    QStringList labels;
    QStringList toolTips;
    QSet<QString> seen;
    for (const QSerialPortInfo &info : ports) {
        const QString name = normalizePortName(info.portName());
        if (name.isEmpty() || seen.contains(name))
            continue;
        seen.insert(name);
        names << name;

        // 同型号 USB 转串口常有多只，描述完全相同；追加序列号或系统位置以便区分。
        QString label = name;
        if (!info.description().isEmpty()
            && descriptionCounts.value(info.description()) > 1) {
            const QString identity = !info.serialNumber().isEmpty()
                                         ? info.serialNumber()
                                         : info.systemLocation();
            if (!identity.isEmpty())
                label += QStringLiteral(" · ") + identity;
        }
        labels << label;
        toolTips << portToolTip(info);
        snapshot << portSnapshotEntry(info);
    }

    // 每 2 秒刷新一次；设备列表没变化时不要重建下拉项，
    // 否则正在展开的列表会被关闭，手动输入的内容也可能被打断。
    if (m_portsInitialized && snapshot == m_portSnapshot) {
        updateOpenButtonState();
        updatePortToolTip();
        return;
    }

    // 用户正在展开列表或输入端口名时，把刷新推迟到下一次，避免抢走焦点。
    if (m_portCombo->view()->isVisible())
        return;
    if (QLineEdit *edit = m_portCombo->lineEdit(); edit && edit->hasFocus())
        return;

    m_portSnapshot = snapshot;
    m_portsInitialized = true;

    const QString previous = selectedPortName();
    const QString saved = normalizePortName(SettingsManager::loadSerialConfig().portName);
    QString preferred;
    if (!previous.isEmpty())
        preferred = previous;
    else if (!m_portUserEdited)
        preferred = saved;

    {
        const QSignalBlocker blocker(m_portCombo);
        m_portCombo->clear();
        for (int i = 0; i < names.size(); ++i) {
            m_portCombo->addItem(labels.at(i), names.at(i));
            m_portCombo->setItemData(i, toolTips.at(i), Qt::ToolTipRole);
        }

        // 保存过但当前未插入的端口仍然保留在列表中，允许用户直接重试打开。
        if (!preferred.isEmpty() && !containsPort(names, preferred)) {
            const int index = m_portCombo->count();
            m_portCombo->addItem(tr("%1（未检测到）").arg(preferred), preferred);
            m_portCombo->setItemData(
                index,
                tr("当前未检测到此串口（可能已拔出）。仍可尝试打开，或手动输入其他端口名。"),
                Qt::ToolTipRole);
        }

        if (preferred.isEmpty()) {
            if (!m_portUserEdited && m_portCombo->count() > 0) {
                m_portCombo->setCurrentIndex(0);
            } else {
                m_portCombo->setCurrentIndex(-1);
                m_portCombo->setEditText(QString());
            }
        } else {
            const int index = findPortRow(preferred);
            if (index >= 0)
                m_portCombo->setCurrentIndex(index);
            else
                m_portCombo->setEditText(preferred);
        }

        if (QLineEdit *edit = m_portCombo->lineEdit()) {
            edit->setPlaceholderText(
                names.isEmpty() ? tr("未检测到串口，可手动输入端口名")
                                : tr("选择或输入端口，如 COM3"));
        }
    }

    updateOpenButtonState();
    updatePortToolTip();
}

SerialConfig SerialConfigPanel::currentConfig() const
{
    SerialConfig config;
    config.portName = selectedPortName();

    qint32 baud = 0;
    config.baudRate = selectedBaudRate(&baud) ? baud : m_committedBaud;
    config.dataBits = static_cast<QSerialPort::DataBits>(
        m_dataBitsCombo->currentData().toInt());
    config.parity = static_cast<QSerialPort::Parity>(m_parityCombo->currentData().toInt());
    config.stopBits = static_cast<QSerialPort::StopBits>(
        m_stopBitsCombo->currentData().toInt());
    config.flowControl = static_cast<QSerialPort::FlowControl>(
        m_flowCombo->currentData().toInt());
    return config;
}

void SerialConfigPanel::applyConfig(const SerialConfig &config)
{
    {
        const QSignalBlocker portBlocker(m_portCombo);
        const QSignalBlocker baudBlocker(m_baudCombo);
        const QSignalBlocker dataBlocker(m_dataBitsCombo);
        const QSignalBlocker parityBlocker(m_parityCombo);
        const QSignalBlocker stopBlocker(m_stopBitsCombo);
        const QSignalBlocker flowBlocker(m_flowCombo);

        const QString portName = normalizePortName(config.portName);
        if (!portName.isEmpty()) {
            const int portIndex = findPortRow(portName);
            if (portIndex >= 0)
                m_portCombo->setCurrentIndex(portIndex);
            else
                m_portCombo->setEditText(portName);
        }

        const qint32 baud = (config.baudRate >= 50 && config.baudRate <= 20000000)
                                ? config.baudRate
                                : 115200;
        m_committedBaud = baud;
        const int baudIndex = m_baudCombo->findData(baud);
        if (baudIndex >= 0)
            m_baudCombo->setCurrentIndex(baudIndex);
        else
            m_baudCombo->setCurrentText(QString::number(baud));

        const int dataIndex = m_dataBitsCombo->findData(config.dataBits);
        if (dataIndex >= 0)
            m_dataBitsCombo->setCurrentIndex(dataIndex);

        const int parityIndex = m_parityCombo->findData(config.parity);
        if (parityIndex >= 0)
            m_parityCombo->setCurrentIndex(parityIndex);

        const int stopIndex = m_stopBitsCombo->findData(config.stopBits);
        if (stopIndex >= 0)
            m_stopBitsCombo->setCurrentIndex(stopIndex);

        const int flowIndex = m_flowCombo->findData(config.flowControl);
        if (flowIndex >= 0)
            m_flowCombo->setCurrentIndex(flowIndex);
    }

    updateOpenButtonState();
    updatePortToolTip();
}

QString SerialConfigPanel::currentPortName() const
{
    return selectedPortName();
}

void SerialConfigPanel::setConnected(bool connected)
{
    m_connected = connected;
    m_openPending = false;
    setControlsEnabled(!connected);

    updateOpenButtonState();

    if (!connected)
        refreshPorts();
    else
        updatePortToolTip();
}

void SerialConfigPanel::setOpenPending(bool pending)
{
    m_openPending = pending;
    if (pending) {
        setControlsEnabled(false);
        m_openButton->setText(tr("正在打开…"));
        m_openButton->setEnabled(false);
        m_openButton->setToolTip(tr("正在等待串口驱动完成打开操作"));
        return;
    }

    if (!m_connected)
        setControlsEnabled(true);
    updateOpenButtonState();
    updatePortToolTip();
}

void SerialConfigPanel::setControlsEnabled(bool enabled)
{
    m_portCombo->setEnabled(enabled);
    m_refreshButton->setEnabled(enabled);
    m_baudCombo->setEnabled(enabled);
    m_dataBitsCombo->setEnabled(enabled);
    m_parityCombo->setEnabled(enabled);
    m_stopBitsCombo->setEnabled(enabled);
    m_flowCombo->setEnabled(enabled);
}

void SerialConfigPanel::onOpenClicked()
{
    if (m_connected) {
        emit closeRequested();
        return;
    }
    if (m_openPending)
        return;

    qint32 baud = 0;
    if (!selectedBaudRate(&baud)) {
        QMessageBox::warning(this, tr("波特率无效"),
                             tr("请输入 50 到 20000000 之间的整数波特率。"));
        m_baudCombo->setFocus();
        if (QLineEdit *edit = m_baudCombo->lineEdit())
            edit->selectAll();
        return;
    }
    m_committedBaud = baud;

    QString frameError;
    if (!validateFrameSettings(&frameError)) {
        QMessageBox::warning(this, tr("串口参数无效"), frameError);
        m_stopBitsCombo->setFocus();
        return;
    }

    const SerialConfig config = currentConfig();
    if (config.portName.isEmpty()) {
        m_portCombo->setFocus();
        if (QLineEdit *edit = m_portCombo->lineEdit())
            edit->setFocus();
        return;
    }

    saveSettings();
    emit openRequested(config);
}

void SerialConfigPanel::restoreSettings()
{
    const SerialConfig config = SettingsManager::loadSerialConfig();
    const qint32 baud = (config.baudRate >= 50 && config.baudRate <= 20000000)
                            ? config.baudRate
                            : 115200;
    m_committedBaud = baud;
    m_baudCombo->setCurrentText(QString::number(baud));

    const int dataIndex = m_dataBitsCombo->findData(config.dataBits);
    if (dataIndex >= 0)
        m_dataBitsCombo->setCurrentIndex(dataIndex);
    const int parityIndex = m_parityCombo->findData(config.parity);
    if (parityIndex >= 0)
        m_parityCombo->setCurrentIndex(parityIndex);
    else
        m_parityCombo->setCurrentIndex(0);
    const int stopIndex = m_stopBitsCombo->findData(config.stopBits);
    if (stopIndex >= 0)
        m_stopBitsCombo->setCurrentIndex(stopIndex);
    else
        m_stopBitsCombo->setCurrentIndex(0);
    const int flowIndex = m_flowCombo->findData(config.flowControl);
    if (flowIndex >= 0)
        m_flowCombo->setCurrentIndex(flowIndex);

    // 控制线与自动重连单独存一组，避免混进串口参数
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("control"));
    m_dtrButton->setChecked(s->value(QStringLiteral("dtr"), false).toBool());
    m_rtsButton->setChecked(s->value(QStringLiteral("rts"), false).toBool());
    m_autoReconnectCheck->setChecked(
        s->value(QStringLiteral("autoReconnect"), false).toBool());
    s->endGroup();
}

void SerialConfigPanel::saveSettings()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("control"));
    s->setValue(QStringLiteral("dtr"), m_dtrButton->isChecked());
    s->setValue(QStringLiteral("rts"), m_rtsButton->isChecked());
    s->setValue(QStringLiteral("autoReconnect"), m_autoReconnectCheck->isChecked());
    s->endGroup();

    if (m_connected || m_openPending)
        return;

    const SerialConfig saved = SettingsManager::loadSerialConfig();
    SerialConfig config = currentConfig();

    // 端口为空时保留上一次的有效端口，其余参数仍照常保存。
    if (config.portName.isEmpty())
        config.portName = saved.portName;
    // 中间输入（如 2、25）不写入设置，只保存最后一次确认的有效波特率。
    config.baudRate = m_committedBaud;

    SettingsManager::saveSerialConfig(config);
}

void SerialConfigPanel::commitBaud()
{
    qint32 baud = 0;
    if (selectedBaudRate(&baud))
        m_committedBaud = baud;
    saveSettings();
}

bool SerialConfigPanel::dtrState() const
{
    return m_dtrButton->isChecked();
}

bool SerialConfigPanel::rtsState() const
{
    return m_rtsButton->isChecked();
}

bool SerialConfigPanel::autoReconnectEnabled() const
{
    return m_autoReconnectCheck->isChecked();
}

QString SerialConfigPanel::selectedPortName() const
{
    // 列表项可能带描述/序列号，而 itemData 保存规范端口名；
    // 用户手动输入时 itemText 与输入文字不同，此时以输入文字为准。
    const int index = m_portCombo->currentIndex();
    if (index >= 0 && m_portCombo->currentText() == m_portCombo->itemText(index)) {
        const QString data = m_portCombo->itemData(index).toString();
        if (!data.isEmpty())
            return normalizePortName(data);
    }
    return normalizePortName(m_portCombo->currentText());
}

int SerialConfigPanel::findPortRow(const QString &portName) const
{
    for (int i = 0; i < m_portCombo->count(); ++i) {
        if (samePortName(m_portCombo->itemData(i).toString(), portName))
            return i;
    }
    return -1;
}

bool SerialConfigPanel::validateFrameSettings(QString *error) const
{
    const auto dataBits = static_cast<QSerialPort::DataBits>(
        m_dataBitsCombo->currentData().toInt());
    const auto stopBits = static_cast<QSerialPort::StopBits>(
        m_stopBitsCombo->currentData().toInt());
    const auto parity = static_cast<QSerialPort::Parity>(
        m_parityCombo->currentData().toInt());

    if (stopBits == QSerialPort::OneAndHalfStop) {
#ifdef Q_OS_WIN
        if (dataBits != QSerialPort::Data5) {
            *error = tr("1.5 位停止位需要配合 5 位数据位使用。\n"
                        "请把数据位改为 5，或把停止位改为 1 / 2。");
            return false;
        }
#else
        *error = tr("1.5 位停止位仅 Windows 平台支持。\n请选择 1 位或 2 位停止位。");
        return false;
#endif
    }

#ifdef Q_OS_MACOS
    if (parity == QSerialPort::MarkParity || parity == QSerialPort::SpaceParity) {
        *error = tr("macOS 不支持标记/空格校验，请选择无校验、奇校验或偶校验。");
        return false;
    }
#endif

    return true;
}

bool SerialConfigPanel::selectedBaudRate(qint32 *baudRate) const
{
    bool ok = false;
    const qlonglong value = m_baudCombo->currentText().trimmed().toLongLong(&ok);
    if (!ok || value < 50 || value > 20000000)
        return false;
    if (baudRate)
        *baudRate = static_cast<qint32>(value);
    return true;
}

void SerialConfigPanel::updateOpenButtonState()
{
    if (m_openPending) {
        m_openButton->setText(tr("正在打开…"));
        m_openButton->setEnabled(false);
        m_openButton->setToolTip(tr("正在等待串口驱动完成打开操作"));
        return;
    }

    const QString portName = selectedPortName();
    const bool hasPort = !portName.isEmpty();
    const bool baudValid = selectedBaudRate(nullptr);

    m_openButton->setText(m_connected ? tr("关闭串口") : tr("打开串口"));
    m_openButton->setEnabled(m_connected || (hasPort && baudValid));

    const bool primary = !m_connected;
    if (m_openButton->property("primary").toBool() != primary) {
        m_openButton->setProperty("primary", primary);
        repolish(m_openButton);
    }

    if (m_connected) {
        m_openButton->setToolTip(tr("关闭当前串口"));
    } else if (!hasPort) {
        m_openButton->setToolTip(tr("请从列表选择串口，或直接输入端口名"));
    } else if (!baudValid) {
        m_openButton->setToolTip(tr("波特率无效：请输入 50–20000000 之间的整数"));
    } else {
        QString frameError;
        if (!validateFrameSettings(&frameError))
            m_openButton->setToolTip(frameError);
        else
            m_openButton->setToolTip(tr("使用当前参数打开 %1").arg(portName));
    }
}

void SerialConfigPanel::updatePortToolTip()
{
    QString tip;
    const int index = m_portCombo->currentIndex();
    if (index >= 0 && m_portCombo->currentText() == m_portCombo->itemText(index))
        tip = m_portCombo->itemData(index, Qt::ToolTipRole).toString();

    if (tip.isEmpty()) {
        const QString custom = selectedPortName();
        tip = custom.isEmpty()
                  ? tr("从列表选择串口，或直接输入端口名（如 COM3、/dev/ttyUSB0）。")
                  : tr("自定义端口：%1。请确认端口名有效。").arg(custom);
    }

    m_portCombo->setToolTip(tip);
    if (QLineEdit *edit = m_portCombo->lineEdit())
        edit->setToolTip(tip);
}
