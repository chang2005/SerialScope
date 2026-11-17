#include "ui/SerialConfigPanel.h"
#include "utils/SettingsManager.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPort>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

class TestSerialConfigPanel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();

    void commonBaudRates();
    void dataBitsParityAndStopBits();
    void manualPortEntryAndRefresh();
    void clearingPortDisablesOpen();
    void decoratedPortItemKeepsCanonicalName();
    void applyConfigUpdatesWidgets();
    void connectionStateLocksConfiguration();
    void openPendingLocksConfiguration();
    void settingsPersistSelection();

private:
    std::unique_ptr<QTemporaryDir> m_settingsDir;
};

void TestSerialConfigPanel::initTestCase()
{
    qRegisterMetaType<SerialConfig>("SerialConfig");

    m_settingsDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_settingsDir->isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir->path());
    QCoreApplication::setOrganizationName(QStringLiteral("SerialScopeTests"));
    QCoreApplication::setApplicationName(QStringLiteral("SerialConfigPanelTests"));
}

void TestSerialConfigPanel::init()
{
    SettingsManager::settings()->clear();
    SettingsManager::settings()->sync();
}

void TestSerialConfigPanel::cleanupTestCase()
{
    SettingsManager::settings()->clear();
    SettingsManager::settings()->sync();
    m_settingsDir.reset();
}

void TestSerialConfigPanel::commonBaudRates()
{
    SerialConfigPanel panel;
    auto *baud = panel.findChild<QComboBox *>(QStringLiteral("baudCombo"));
    QVERIFY(baud);
    QVERIFY(baud->isEditable());

    const QList<int> expected = {
        300,    600,    1200,   2400,   4800,   9600,    14400,   19200,
        38400,  57600,  115200, 128000, 230400, 256000,  460800,  500000,
        576000, 921600, 1000000, 1152000, 1500000, 2000000, 3000000, 4000000
    };
    for (int rate : expected)
        QVERIFY2(baud->findData(rate) >= 0, qPrintable(QString::number(rate)));

    baud->setCurrentText(QStringLiteral("250000"));
    QCOMPARE(panel.currentConfig().baudRate, 250000);

    baud->setCurrentText(QStringLiteral("921600"));
    QCOMPARE(panel.currentConfig().baudRate, 921600);
}

void TestSerialConfigPanel::dataBitsParityAndStopBits()
{
    SerialConfigPanel panel;
    auto *data = panel.findChild<QComboBox *>(QStringLiteral("dataBitsCombo"));
    auto *parity = panel.findChild<QComboBox *>(QStringLiteral("parityCombo"));
    auto *stop = panel.findChild<QComboBox *>(QStringLiteral("stopBitsCombo"));
    auto *flow = panel.findChild<QComboBox *>(QStringLiteral("flowCombo"));
    QVERIFY(data);
    QVERIFY(parity);
    QVERIFY(stop);
    QVERIFY(flow);

    const QList<int> dataBits = {QSerialPort::Data5, QSerialPort::Data6,
                                 QSerialPort::Data7, QSerialPort::Data8};
    QCOMPARE(data->count(), dataBits.size());
    for (int i = 0; i < dataBits.size(); ++i) {
        QCOMPARE(data->itemData(i).toInt(), dataBits.at(i));
        data->setCurrentIndex(i);
        QCOMPARE(static_cast<int>(panel.currentConfig().dataBits), dataBits.at(i));
    }

    const QList<int> parities = {QSerialPort::NoParity, QSerialPort::OddParity,
                                 QSerialPort::EvenParity, QSerialPort::MarkParity,
                                 QSerialPort::SpaceParity};
    QCOMPARE(parity->count(), parities.size());
    for (int i = 0; i < parities.size(); ++i) {
        QCOMPARE(parity->itemData(i).toInt(), parities.at(i));
        parity->setCurrentIndex(i);
        QCOMPARE(static_cast<int>(panel.currentConfig().parity), parities.at(i));
    }

    const QList<int> stopBits = {QSerialPort::OneStop, QSerialPort::OneAndHalfStop,
                                 QSerialPort::TwoStop};
    QCOMPARE(stop->count(), stopBits.size());
    for (int i = 0; i < stopBits.size(); ++i) {
        QCOMPARE(stop->itemData(i).toInt(), stopBits.at(i));
        stop->setCurrentIndex(i);
        QCOMPARE(static_cast<int>(panel.currentConfig().stopBits), stopBits.at(i));
    }

    QCOMPARE(flow->count(), 3);
}

void TestSerialConfigPanel::manualPortEntryAndRefresh()
{
    SerialConfigPanel panel;
    auto *port = panel.findChild<QComboBox *>(QStringLiteral("portCombo"));
    auto *open = panel.findChild<QPushButton *>(QStringLiteral("openButton"));
    QVERIFY(port);
    QVERIFY(open);
    QVERIFY(port->isEditable());

    port->setEditText(QStringLiteral("COM250"));
    QCoreApplication::processEvents();
    QCOMPARE(panel.currentPortName(), QStringLiteral("COM250"));
    QVERIFY(open->isEnabled());

    panel.refreshPorts();
    QCOMPARE(panel.currentPortName(), QStringLiteral("COM250"));
    QVERIFY(open->isEnabled());

    QSignalSpy spy(&panel, &SerialConfigPanel::openRequested);
    QTest::mouseClick(open, Qt::LeftButton);
    QCOMPARE(spy.count(), 1);
    const SerialConfig config = qvariant_cast<SerialConfig>(spy.takeFirst().at(0));
    QCOMPARE(config.portName, QStringLiteral("COM250"));
    QCOMPARE(config.baudRate, 115200);
}

void TestSerialConfigPanel::clearingPortDisablesOpen()
{
    SerialConfigPanel panel;
    auto *port = panel.findChild<QComboBox *>(QStringLiteral("portCombo"));
    auto *open = panel.findChild<QPushButton *>(QStringLiteral("openButton"));
    QVERIFY(port);
    QVERIFY(open);

    port->setEditText(QStringLiteral("COM250"));
    QCoreApplication::processEvents();
    QVERIFY(open->isEnabled());

    port->clearEditText();
    QCoreApplication::processEvents();
    QVERIFY(panel.currentPortName().isEmpty());
    QVERIFY(!open->isEnabled());
}

void TestSerialConfigPanel::decoratedPortItemKeepsCanonicalName()
{
    SerialConfigPanel panel;
    auto *port = panel.findChild<QComboBox *>(QStringLiteral("portCombo"));
    auto *open = panel.findChild<QPushButton *>(QStringLiteral("openButton"));
    QVERIFY(port);
    QVERIFY(open);

    port->addItem(QStringLiteral("COM250 · USB-SERIAL CH340"), QStringLiteral("COM250"));
    port->setCurrentIndex(port->count() - 1);
    QCoreApplication::processEvents();

    QCOMPARE(panel.currentPortName(), QStringLiteral("COM250"));
    QVERIFY(open->isEnabled());
}

void TestSerialConfigPanel::applyConfigUpdatesWidgets()
{
    SerialConfigPanel panel;

    SerialConfig expected;
    expected.portName = QStringLiteral("COM250");
    expected.baudRate = 256000;
    expected.dataBits = QSerialPort::Data7;
    expected.parity = QSerialPort::EvenParity;
    expected.stopBits = QSerialPort::TwoStop;
    expected.flowControl = QSerialPort::HardwareControl;

    panel.applyConfig(expected);
    QCOMPARE(panel.currentConfig(), expected);
}

void TestSerialConfigPanel::connectionStateLocksConfiguration()
{
    SerialConfigPanel panel;
    auto *port = panel.findChild<QComboBox *>(QStringLiteral("portCombo"));
    auto *baud = panel.findChild<QComboBox *>(QStringLiteral("baudCombo"));
    auto *data = panel.findChild<QComboBox *>(QStringLiteral("dataBitsCombo"));
    auto *parity = panel.findChild<QComboBox *>(QStringLiteral("parityCombo"));
    auto *stop = panel.findChild<QComboBox *>(QStringLiteral("stopBitsCombo"));
    auto *flow = panel.findChild<QComboBox *>(QStringLiteral("flowCombo"));
    auto *open = panel.findChild<QPushButton *>(QStringLiteral("openButton"));
    QVERIFY(port && baud && data && parity && stop && flow && open);

    port->setEditText(QStringLiteral("COM250"));
    panel.setConnected(true);
    QVERIFY(!port->isEnabled());
    QVERIFY(!baud->isEnabled());
    QVERIFY(!data->isEnabled());
    QVERIFY(!parity->isEnabled());
    QVERIFY(!stop->isEnabled());
    QVERIFY(!flow->isEnabled());
    QCOMPARE(open->text(), QStringLiteral("关闭串口"));
    QVERIFY(open->isEnabled());

    QSignalSpy closeSpy(&panel, &SerialConfigPanel::closeRequested);
    QTest::mouseClick(open, Qt::LeftButton);
    QCOMPARE(closeSpy.count(), 1);

    panel.setConnected(false);
    QVERIFY(port->isEnabled());
    QVERIFY(baud->isEnabled());
    QVERIFY(data->isEnabled());
    QVERIFY(parity->isEnabled());
    QVERIFY(stop->isEnabled());
    QVERIFY(flow->isEnabled());
    QCOMPARE(open->text(), QStringLiteral("打开串口"));
    QVERIFY(open->isEnabled());
}

void TestSerialConfigPanel::openPendingLocksConfiguration()
{
    SerialConfigPanel panel;
    auto *port = panel.findChild<QComboBox *>(QStringLiteral("portCombo"));
    auto *baud = panel.findChild<QComboBox *>(QStringLiteral("baudCombo"));
    auto *open = panel.findChild<QPushButton *>(QStringLiteral("openButton"));
    QVERIFY(port && baud && open);

    port->setEditText(QStringLiteral("COM250"));
    QCoreApplication::processEvents();
    QVERIFY(open->isEnabled());

    panel.setOpenPending(true);
    QVERIFY(!port->isEnabled());
    QVERIFY(!baud->isEnabled());
    QVERIFY(!open->isEnabled());
    QCOMPARE(open->text(), QStringLiteral("正在打开…"));

    panel.setOpenPending(false);
    QVERIFY(port->isEnabled());
    QVERIFY(baud->isEnabled());
    QVERIFY(open->isEnabled());
    QCOMPARE(open->text(), QStringLiteral("打开串口"));
}

void TestSerialConfigPanel::settingsPersistSelection()
{
    {
        SerialConfigPanel panel;
        auto *port = panel.findChild<QComboBox *>(QStringLiteral("portCombo"));
        auto *baud = panel.findChild<QComboBox *>(QStringLiteral("baudCombo"));
        auto *data = panel.findChild<QComboBox *>(QStringLiteral("dataBitsCombo"));
        auto *parity = panel.findChild<QComboBox *>(QStringLiteral("parityCombo"));
        auto *stop = panel.findChild<QComboBox *>(QStringLiteral("stopBitsCombo"));
        auto *flow = panel.findChild<QComboBox *>(QStringLiteral("flowCombo"));
        auto *open = panel.findChild<QPushButton *>(QStringLiteral("openButton"));
        QVERIFY(port && baud && data && parity && stop && flow && open);

        port->setEditText(QStringLiteral("COM250"));
        baud->setCurrentText(QStringLiteral("256000"));
        data->setCurrentIndex(data->findData(QSerialPort::Data7));
        parity->setCurrentIndex(parity->findData(QSerialPort::EvenParity));
        stop->setCurrentIndex(stop->findData(QSerialPort::TwoStop));
        flow->setCurrentIndex(flow->findData(QSerialPort::HardwareControl));
        QCoreApplication::processEvents();

        QSignalSpy spy(&panel, &SerialConfigPanel::openRequested);
        QTest::mouseClick(open, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);
    }

    SerialConfigPanel restored;
    const SerialConfig config = restored.currentConfig();
    QCOMPARE(config.portName, QStringLiteral("COM250"));
    QCOMPARE(config.baudRate, 256000);
    QCOMPARE(static_cast<int>(config.dataBits), static_cast<int>(QSerialPort::Data7));
    QCOMPARE(static_cast<int>(config.parity), static_cast<int>(QSerialPort::EvenParity));
    QCOMPARE(static_cast<int>(config.stopBits), static_cast<int>(QSerialPort::TwoStop));
    QCOMPARE(static_cast<int>(config.flowControl),
             static_cast<int>(QSerialPort::HardwareControl));
}

QTEST_MAIN(TestSerialConfigPanel)
#include "test_serial_config.moc"
