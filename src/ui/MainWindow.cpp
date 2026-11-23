#include "ui/MainWindow.h"

#include "core/DataLogger.h"
#include "core/SerialWorker.h"
#include "ui/ReceivePanel.h"
#include "ui/SendPanel.h"
#include "ui/SerialConfigPanel.h"
#include "ui/Theme.h"
#include "ui/WaveformPanel.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/AutoReplyDialog.h"
#include "ui/dialogs/CrcToolDialog.h"
#include "ui/dialogs/EncodingFixDialog.h"
#include "utils/SettingsManager.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace {

QString formatBytes(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    const double kb = bytes / 1024.0;
    if (kb < 1024.0)
        return QStringLiteral("%1 KB").arg(kb, 0, 'f', kb < 10.0 ? 2 : 1);
    const double mb = kb / 1024.0;
    if (mb < 1024.0)
        return QStringLiteral("%1 MB").arg(mb, 0, 'f', mb < 10.0 ? 2 : 1);
    return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
}

QString formatRate(double bytesPerSecond)
{
    if (bytesPerSecond < 1.0)
        return QStringLiteral("0 B");
    return formatBytes(static_cast<qint64>(bytesPerSecond));
}

QString sanitizedPortName(const QString &name)
{
    QString safe;
    safe.reserve(name.size());
    for (const QChar ch : name) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_'))
            safe.append(ch);
        else
            safe.append(QLatin1Char('_'));
    }
    if (safe.isEmpty())
        safe = QStringLiteral("port");
    return safe;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupToolbar();
    setupThreads();
    setupConnections();
    restoreWindowState();
    restoreAutoReplyRules();
    updateConnectionStatus();
    updateStatsLabels();
    updateLogLabel();

    // 首次显示后按实际宽度分配分栏比例（窗口尚未显示时 splitter 宽度为 0）
    QTimer::singleShot(0, this, [this] {
        if (!m_hasSavedSplitterState)
            applyDefaultSplitterSizes();
    });

    statusBar()->showMessage(tr("就绪"), 3000);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MainWindow::shutdown);
}

MainWindow::~MainWindow()
{
    shutdown();
}

void MainWindow::setupUi()
{
    setWindowTitle(QStringLiteral("SerialScope"));

    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    const QRect available = primaryScreen
                                ? primaryScreen->availableGeometry()
                                : QRect(0, 0, 1380, 860);
    const int minW = qMin(1024, available.width());
    const int minH = qMin(640, available.height());
    setMinimumSize(minW, minH);
    const int width = qBound(minW, available.width() - 50, 1380);
    const int height = qBound(minH, available.height() - 50, 860);
    resize(width, height);
    move(qMax(0, (available.width() - width) / 2),
         qMax(0, (available.height() - height) / 2));

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_configPanel = new SerialConfigPanel(central);
    layout->addWidget(m_configPanel);

    m_mainSplitter = new QSplitter(Qt::Horizontal, central);
    // 注意：分栏子控件先不指定 splitter 作为 parent，
    // 否则它们可能在 addWidget 之前就被 QSplitter 接管，造成重复插入。
    m_leftSplitter = new QSplitter(Qt::Vertical);

    m_receivePanel = new ReceivePanel;
    m_sendPanel = new SendPanel;
    m_sendPanel->setMinimumHeight(190);
    m_leftSplitter->addWidget(m_receivePanel);
    m_leftSplitter->addWidget(m_sendPanel);
    m_leftSplitter->setStretchFactor(0, 3);
    m_leftSplitter->setStretchFactor(1, 1);
    m_leftSplitter->setChildrenCollapsible(false);

    m_waveformPanel = new WaveformPanel;
    m_waveformPanel->setMinimumWidth(380);

    m_mainSplitter->addWidget(m_leftSplitter);
    m_mainSplitter->addWidget(m_waveformPanel);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 2);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setSizes({840, 560});
    m_leftSplitter->setSizes({540, 280});

    layout->addWidget(m_mainSplitter, 1);
    setCentralWidget(central);

    m_connLabel = new QLabel(tr("未连接"), this);
    m_txLabel = new QLabel(tr("TX 0 B"), this);
    m_rxLabel = new QLabel(tr("RX 0 B"), this);
    m_rateLabel = new QLabel(QStringLiteral("↑ 0 B/s   ↓ 0 B/s"), this);
    m_logLabel = new QLabel(QString(), this);

    const QFont mono = Theme::monospaceFont(9);
    m_txLabel->setFont(mono);
    m_rxLabel->setFont(mono);
    m_rateLabel->setFont(mono);
    m_txLabel->setProperty("muted", true);
    m_rxLabel->setProperty("muted", true);
    m_rateLabel->setProperty("muted", true);
    m_logLabel->setProperty("muted", true);

    statusBar()->addWidget(m_connLabel, 1);
    statusBar()->addPermanentWidget(m_txLabel);
    statusBar()->addPermanentWidget(m_rxLabel);
    statusBar()->addPermanentWidget(m_rateLabel);
    statusBar()->addPermanentWidget(m_logLabel);
    statusBar()->setSizeGripEnabled(true);
}

void MainWindow::setupToolbar()
{
    auto *fileMenu = menuBar()->addMenu(tr("文件(&F)"));

    auto *saveViewAction = new QAction(tr("保存显示内容…"), this);
    saveViewAction->setShortcut(QKeySequence::Save);
    connect(saveViewAction, &QAction::triggered, m_receivePanel, &ReceivePanel::saveDisplay);
    fileMenu->addAction(saveViewAction);
    fileMenu->addSeparator();

    m_logAction = new QAction(style()->standardIcon(QStyle::SP_DialogSaveButton),
                              tr("记录日志"), this);
    m_logAction->setCheckable(true);
    m_logAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    m_logAction->setToolTip(tr("把接收与发送数据写入日志文件（Ctrl+L）"));
    m_logAction->setEnabled(false);
    fileMenu->addAction(m_logAction);

    m_autoLogAction = new QAction(style()->standardIcon(QStyle::SP_DialogApplyButton),
                                  tr("自动记录"), this);
    m_autoLogAction->setCheckable(true);
    m_autoLogAction->setToolTip(tr("每次打开串口时自动开始记录日志"));
    m_autoLogAction->setChecked(
        SettingsManager::settings()->value(QStringLiteral("logging/auto"), false).toBool());
    fileMenu->addAction(m_autoLogAction);

    fileMenu->addSeparator();
    auto *quitAction = new QAction(tr("退出"), this);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(quitAction);

    auto *toolsMenu = menuBar()->addMenu(tr("工具(&T)"));

    m_crcAction = new QAction(style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                              tr("CRC / 校验工具…"), this);
    connect(m_crcAction, &QAction::triggered, this, &MainWindow::showCrcTool);
    toolsMenu->addAction(m_crcAction);

    m_encodingAction = new QAction(style()->standardIcon(QStyle::SP_FileDialogContentsView),
                                   tr("编码转换 / 乱码修复…"), this);
    m_encodingAction->setToolTip(
        tr("在 GBK / UTF-8 / ASCII / Latin-1 之间转换，辅助修复编码不匹配造成的乱码"));
    connect(m_encodingAction, &QAction::triggered, this, &MainWindow::showEncodingTool);
    toolsMenu->addAction(m_encodingAction);

    m_autoReplyAction = new QAction(style()->standardIcon(QStyle::SP_BrowserReload),
                                    tr("自动回复规则…"), this);
    m_autoReplyAction->setCheckable(true);
    m_autoReplyAction->setChecked(true);
    m_autoReplyAction->setToolTip(
        tr("开启后，收到匹配的数据会自动回复预设内容（模拟从机设备）"));
    connect(m_autoReplyAction, &QAction::toggled, this, &MainWindow::onAutoReplyToggled);
    toolsMenu->addAction(m_autoReplyAction);

    m_themeAction = new QAction(style()->standardIcon(QStyle::SP_DesktopIcon),
                                tr("深色模式"), this);
    m_themeAction->setCheckable(true);
    m_themeAction->setChecked(Theme::isDark());
    connect(m_themeAction, &QAction::toggled, this, &MainWindow::toggleTheme);
    toolsMenu->addAction(m_themeAction);

    auto *helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    auto *aboutAction = new QAction(style()->standardIcon(QStyle::SP_MessageBoxInformation),
                                    tr("关于 SerialScope…"), this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(aboutAction);

    auto *toolbar = addToolBar(tr("工具栏"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(16, 16));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(m_logAction);
    toolbar->addAction(m_autoLogAction);
    toolbar->addSeparator();
    toolbar->addAction(m_crcAction);
    toolbar->addAction(m_autoReplyAction);
    toolbar->addAction(m_themeAction);
    toolbar->addSeparator();
    toolbar->addAction(aboutAction);

    connect(m_logAction, &QAction::toggled, this, &MainWindow::onLogActionToggled);
    connect(m_autoLogAction, &QAction::toggled, this, &MainWindow::onAutoLogToggled);
}

void MainWindow::setupThreads()
{
    m_ioThread = new QThread(this);
    m_ioThread->setObjectName(QStringLiteral("serial-io"));
    m_worker = new SerialWorker;
    m_worker->moveToThread(m_ioThread);
    connect(m_ioThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_ioThread->start();

    m_logThread = new QThread(this);
    m_logThread->setObjectName(QStringLiteral("data-logger"));
    m_logger = new DataLogger;
    m_logger->moveToThread(m_logThread);
    connect(m_logThread, &QThread::finished, m_logger, &QObject::deleteLater);
    m_logThread->start();
}

void MainWindow::setupConnections()
{
    connect(m_configPanel, &SerialConfigPanel::openRequested,
            this, &MainWindow::onOpenRequested);
    connect(m_configPanel, &SerialConfigPanel::closeRequested,
            this, &MainWindow::onCloseRequested);
    connect(m_configPanel, &SerialConfigPanel::dtrChanged,
            m_worker, &SerialWorker::setDtr);
    connect(m_configPanel, &SerialConfigPanel::rtsChanged,
            m_worker, &SerialWorker::setRts);
    connect(m_configPanel, &SerialConfigPanel::autoReconnectChanged,
            m_worker, &SerialWorker::setAutoReconnect);
    connect(m_worker, &SerialWorker::reconnectAttempted, this,
            &MainWindow::onReconnectAttempted);
    connect(m_worker, &SerialWorker::controlLinesChanged, this,
            &MainWindow::onControlLinesChanged);

    connect(m_sendPanel, &SendPanel::dataToSend, m_worker, &SerialWorker::sendData);
    connect(m_sendPanel, &SendPanel::dataToSend, this, &MainWindow::onSendEcho);
    connect(m_sendPanel, &SendPanel::fileSendRequested, m_worker, &SerialWorker::sendFile);
    connect(m_sendPanel, &SendPanel::cancelFileSendRequested,
            m_worker, &SerialWorker::cancelFileSend);
    connect(m_worker, &SerialWorker::fileSendProgress,
            m_sendPanel, &SendPanel::setFileProgress);
    connect(m_worker, &SerialWorker::fileSendFinished,
            m_sendPanel, &SendPanel::onFileSendFinished);
    connect(m_worker, &SerialWorker::txBackpressure, m_sendPanel, &SendPanel::onTxBackpressure);
    connect(m_worker, &SerialWorker::txBackpressure, this, [this](qint64 queuedBytes) {
        showStatus(tr("发送缓冲积压 %1 字节，已暂缓写入").arg(queuedBytes));
    });

    connect(m_worker, &SerialWorker::portOpened, this, &MainWindow::onPortOpened);
    connect(m_worker, &SerialWorker::portClosed, this, &MainWindow::onPortClosed);
    connect(m_worker, &SerialWorker::portLost, this, &MainWindow::onPortLost);
    connect(m_worker, &SerialWorker::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(m_worker, &SerialWorker::rxDataAvailable, this, &MainWindow::onRxDataAvailable);
    connect(m_worker, &SerialWorker::statsUpdated, this, &MainWindow::onStatsUpdated);

    connect(m_logger, &DataLogger::started, this, &MainWindow::onLogStarted);
    connect(m_logger, &DataLogger::stopped, this, &MainWindow::onLogStopped);
    connect(m_logger, &DataLogger::errorOccurred, this, &MainWindow::onLogError);
    connect(m_logger, &DataLogger::overflow, this, &MainWindow::onLogOverflow);
    connect(m_logger, &DataLogger::bytesWritten, this, &MainWindow::onLogBytesWritten);

    connect(m_sendPanel, &SendPanel::statusMessage, this, &MainWindow::showStatus);
    connect(m_waveformPanel, &WaveformPanel::statusMessage, this, &MainWindow::showStatus);

    m_hotplugTimer = new QTimer(this);
    m_hotplugTimer->setInterval(2000);
    connect(m_hotplugTimer, &QTimer::timeout, this, &MainWindow::refreshHotplug);
    m_hotplugTimer->start();

    m_rateTimer = new QTimer(this);
    m_rateTimer->setInterval(1000);
    connect(m_rateTimer, &QTimer::timeout, this, &MainWindow::updateRates);
    m_rateTimer->start();
}

void MainWindow::restoreWindowState()
{
    QSettings *settings = SettingsManager::settings();
    settings->beginGroup(QStringLiteral("window"));
    const QByteArray geometry = settings->value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    const QByteArray state = settings->value(QStringLiteral("state")).toByteArray();
    if (!state.isEmpty())
        restoreState(state);
    const QByteArray mainSplit = settings->value(QStringLiteral("mainSplitter")).toByteArray();
    if (!mainSplit.isEmpty()) {
        m_mainSplitter->restoreState(mainSplit);
        m_hasSavedSplitterState = true;
    }
    const QByteArray leftSplit = settings->value(QStringLiteral("leftSplitter")).toByteArray();
    if (!leftSplit.isEmpty())
        m_leftSplitter->restoreState(leftSplit);
    settings->endGroup();
}

void MainWindow::applyDefaultSplitterSizes()
{
    const int totalWidth = m_mainSplitter->width();
    if (totalWidth < 600)
        return;

    const int handle = m_mainSplitter->handleWidth();
    int waveform = qBound(360, static_cast<int>(totalWidth * 0.38), 620);
    if (totalWidth - waveform - handle < 480)
        waveform = qMax(300, totalWidth - 480 - handle);
    int left = totalWidth - waveform - handle;
    if (left < 480) {
        left = 480;
        waveform = qMax(260, totalWidth - left - handle);
    }
    m_mainSplitter->setSizes({left, waveform});

    const int leftHeight = m_leftSplitter->height();
    if (leftHeight > 400) {
        const int leftHandle = m_leftSplitter->handleWidth();
        int send = qBound(190, static_cast<int>(leftHeight * 0.30), 300);
        int receive = leftHeight - send - leftHandle;
        if (receive < 240) {
            receive = 240;
            send = qMax(170, leftHeight - receive - leftHandle);
        }
        m_leftSplitter->setSizes({receive, send});
    }
}

void MainWindow::saveWindowState()
{
    QSettings *settings = SettingsManager::settings();
    settings->beginGroup(QStringLiteral("window"));
    settings->setValue(QStringLiteral("geometry"), saveGeometry());
    settings->setValue(QStringLiteral("state"), saveState());
    settings->setValue(QStringLiteral("mainSplitter"), m_mainSplitter->saveState());
    settings->setValue(QStringLiteral("leftSplitter"), m_leftSplitter->saveState());
    settings->endGroup();
    SettingsManager::save();
}

void MainWindow::onOpenRequested(const SerialConfig &config)
{
    if (m_openPending || m_connected) {
        showStatus(tr("串口已打开或正在打开"));
        return;
    }

    m_currentConfig = config;
    m_openPending = true;
    m_configPanel->setOpenPending(true);
    showStatus(tr("正在打开 %1…").arg(config.portName));

    // 先把控制线与自动重连状态同步给工作线程，再打开端口
    QMetaObject::invokeMethod(m_worker, "setAutoReconnect", Qt::QueuedConnection,
                               Q_ARG(bool, m_configPanel->autoReconnectEnabled()));
    QMetaObject::invokeMethod(m_worker, "setDtr", Qt::QueuedConnection,
                               Q_ARG(bool, m_configPanel->dtrState()));
    QMetaObject::invokeMethod(m_worker, "setRts", Qt::QueuedConnection,
                               Q_ARG(bool, m_configPanel->rtsState()));

    if (!QMetaObject::invokeMethod(m_worker, "openPort", Qt::QueuedConnection,
                                   Q_ARG(SerialConfig, config))) {
        m_openPending = false;
        m_configPanel->setOpenPending(false);
        showStatus(tr("无法调用串口工作线程"));
    }
}

void MainWindow::onCloseRequested()
{
    if (!m_connected && !m_openPending)
        return;
    showStatus(tr("正在关闭串口…"));
    QMetaObject::invokeMethod(m_worker, "closePort", Qt::QueuedConnection);
}

void MainWindow::onPortOpened(const QString &portName, qint32 baudRate)
{
    if (m_shuttingDown)
        return;

    m_openPending = false;
    m_connected = true;
    m_currentConfig.portName = portName;
    m_currentConfig.baudRate = baudRate;

    m_configPanel->setOpenPending(false);
    m_configPanel->applyConfig(m_currentConfig);
    m_configPanel->setConnected(true);
    SettingsManager::saveSerialConfig(m_currentConfig);
    m_sendPanel->setPortOpen(true);
    m_txTotal = 0;
    m_rxTotal = 0;
    m_lastTxSample = 0;
    m_lastRxSample = 0;
    m_txRate = 0.0;
    m_rxRate = 0.0;
    updateStatsLabels();
    updateConnectionStatus();
    m_logAction->setEnabled(true);

    setWindowTitle(tr("SerialScope — %1 @ %2").arg(portName).arg(baudRate));
    m_receivePanel->appendSystemLine(
        tr("── 已打开 %1 ──").arg(configSummary(m_currentConfig)));
    showStatus(tr("串口已打开：%1").arg(configSummary(m_currentConfig)));

    if (m_autoLogAction->isChecked() && !m_logActive && !m_logRequestPending)
        beginLog(makeLogPath(portName), true);
}

void MainWindow::onPortClosed()
{
    if (m_shuttingDown)
        return;

    const bool wasConnected = m_connected;
    m_connected = false;
    m_openPending = false;
    m_configPanel->setConnected(false);
    m_sendPanel->setPortOpen(false);
    m_logAction->setEnabled(false);

    if (wasConnected)
        m_receivePanel->appendSystemLine(tr("── 串口已关闭 ──"));
    if (m_logActive || m_logRequestPending)
        endLog();

    setWindowTitle(QStringLiteral("SerialScope"));
    updateConnectionStatus();
    showStatus(tr("串口已关闭"));
}

void MainWindow::onPortLost(const QString &message)
{
    if (m_shuttingDown)
        return;

    m_openPending = false;
    m_receivePanel->appendSystemLine(tr("── 串口异常断开：%1 ──").arg(message));
    m_connLabel->setText(tr("连接已断开"));
    m_connLabel->setStyleSheet(QStringLiteral("color:#D1242F;font-weight:600;"));
    showStatus(tr("串口连接丢失：%1").arg(message));

    // 开启自动重连时不弹模态框，避免打断恢复流程
    if (m_configPanel && m_configPanel->autoReconnectEnabled()) {
        m_receivePanel->appendSystemLine(tr("── 正在尝试自动重连… ──"));
        return;
    }

    QMessageBox::warning(this, tr("串口已断开"),
                         tr("串口连接已丢失：\n%1\n\n请检查设备连接后重新打开串口。").arg(message));
}

void MainWindow::onReconnectAttempted(int attempt, bool success, const QString &message)
{
    if (m_shuttingDown)
        return;

    if (success) {
        m_receivePanel->appendSystemLine(
            tr("── 自动重连成功（第 %1 次尝试） ──").arg(attempt));
        showStatus(tr("串口已自动重连"));
        return;
    }

    m_receivePanel->appendSystemLine(
        tr("── 自动重连第 %1 次失败：%2 ──").arg(attempt).arg(message));
}

void MainWindow::onControlLinesChanged(bool dtr, bool rts)
{
    if (m_shuttingDown || !m_connLabel)
        return;

    const QString state = tr("DTR %1 · RTS %2")
                              .arg(dtr ? tr("置位") : tr("复位"),
                                   rts ? tr("置位") : tr("复位"));
    m_connLabel->setToolTip(state);
}

void MainWindow::onErrorOccurred(const QString &message)
{
    if (m_shuttingDown)
        return;

    if (m_openPending) {
        m_openPending = false;
        m_configPanel->setOpenPending(false);
        m_connLabel->setText(tr("未连接"));
        m_connLabel->setStyleSheet(QString());
        QMessageBox::warning(this, tr("打开串口失败"), message);
        showStatus(message);
        return;
    }

    showStatus(message);
    m_receivePanel->appendSystemLine(tr("── 错误：%1 ──").arg(message));
}

void MainWindow::onRxDataAvailable()
{
    if (!m_worker || m_shuttingDown)
        return;

    qint64 droppedBytes = 0;
    const QByteArray data = m_worker->takeRxData(&droppedBytes);
    if (droppedBytes > 0)
        showStatus(tr("接收缓冲区溢出，已丢弃 %1 字节").arg(droppedBytes));
    if (!data.isEmpty())
        handleIncomingData(data);
}

void MainWindow::handleIncomingData(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    m_receivePanel->appendRx(data);
    m_waveformPanel->feed(data);
    if (m_logger)
        m_logger->appendRx(data); // 未记录时内部直接返回

    // 轮发序列的「应答触发」模式：收到数据才发送下一条
    m_sendPanel->onDataReceived();

    // 自动回复：收到匹配的数据后自动回一条预设内容
    if (m_autoReplyEnabled && !m_autoReplyRules.isEmpty()) {
        AutoReplyRule matched;
        QByteArray reply;
        if (m_autoReplyRules.match(data, &matched, &reply) && !reply.isEmpty()) {
            QMetaObject::invokeMethod(m_worker, "sendData", Qt::QueuedConnection,
                                      Q_ARG(QByteArray, reply));
            m_receivePanel->appendSystemLine(
                tr("── 自动回复（规则：%1）：%2 字节 ──")
                    .arg(matched.name).arg(reply.size()));
        }
    }
}

void MainWindow::onSendEcho(const QByteArray &data)
{
    if (data.isEmpty())
        return;
    m_receivePanel->appendTx(data);
    if (m_logger)
        m_logger->appendTx(data);
}

void MainWindow::onStatsUpdated(qint64 txBytes, qint64 rxBytes)
{
    m_txTotal = txBytes;
    m_rxTotal = rxBytes;
    updateStatsLabels();
}

void MainWindow::updateRates()
{
    m_txRate = static_cast<double>(qMax<qint64>(0, m_txTotal - m_lastTxSample));
    m_rxRate = static_cast<double>(qMax<qint64>(0, m_rxTotal - m_lastRxSample));
    m_lastTxSample = m_txTotal;
    m_lastRxSample = m_rxTotal;
    updateStatsLabels();
}

void MainWindow::updateStatsLabels()
{
    if (!m_txLabel)
        return;
    m_txLabel->setText(tr("TX %1").arg(formatBytes(m_txTotal)));
    m_rxLabel->setText(tr("RX %1").arg(formatBytes(m_rxTotal)));
    m_rateLabel->setText(tr("↑ %1/s   ↓ %2/s")
                            .arg(formatRate(m_txRate), formatRate(m_rxRate)));
}

void MainWindow::updateConnectionStatus()
{
    if (!m_connLabel)
        return;

    if (m_connected) {
        m_connLabel->setText(tr("已连接 · %1").arg(configSummary(m_currentConfig)));
        m_connLabel->setStyleSheet(QStringLiteral("color:#2EA043;font-weight:600;"));
        m_connLabel->setToolTip(m_currentConfig.portName);
    } else {
        m_connLabel->setText(tr("未连接"));
        m_connLabel->setStyleSheet(QString());
        m_connLabel->setToolTip(QString());
    }
    if (m_logAction)
        m_logAction->setEnabled(m_connected);
}

QString MainWindow::configSummary(const SerialConfig &config) const
{
    QString parity;
    switch (config.parity) {
    case QSerialPort::NoParity: parity = QStringLiteral("N"); break;
    case QSerialPort::EvenParity: parity = QStringLiteral("E"); break;
    case QSerialPort::OddParity: parity = QStringLiteral("O"); break;
    case QSerialPort::SpaceParity: parity = QStringLiteral("S"); break;
    case QSerialPort::MarkParity: parity = QStringLiteral("M"); break;
    }

    QString stop;
    switch (config.stopBits) {
    case QSerialPort::OneStop: stop = QStringLiteral("1"); break;
    case QSerialPort::OneAndHalfStop: stop = QStringLiteral("1.5"); break;
    case QSerialPort::TwoStop: stop = QStringLiteral("2"); break;
    }

    QString flow;
    if (config.flowControl == QSerialPort::HardwareControl)
        flow = tr(" · RTS/CTS");
    else if (config.flowControl == QSerialPort::SoftwareControl)
        flow = tr(" · XON/XOFF");

    return QStringLiteral("%1 · %2 · %3%4%5%6")
        .arg(config.portName)
        .arg(config.baudRate)
        .arg(config.dataBits)
        .arg(parity, stop, flow);
}

void MainWindow::onLogActionToggled(bool enabled)
{
    if (enabled) {
        if (m_logActive || m_logRequestPending)
            return;

        QString path = makeLogPath(m_currentConfig.portName);
        path = QFileDialog::getSaveFileName(this, tr("选择日志文件"), path,
                                            tr("日志文件 (*.log);;文本文件 (*.txt);;所有文件 (*)"));
        if (path.isEmpty()) {
            QSignalBlocker blocker(m_logAction);
            m_logAction->setChecked(false);
            return;
        }
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".log");
        beginLog(path, false);
    } else if (m_logActive || m_logRequestPending) {
        endLog();
    }
}

void MainWindow::onAutoLogToggled(bool enabled)
{
    SettingsManager::settings()->setValue(QStringLiteral("logging/auto"), enabled);
    if (enabled && m_connected && !m_logActive && !m_logRequestPending)
        beginLog(makeLogPath(m_currentConfig.portName), true);
}

void MainWindow::beginLog(const QString &path, bool automatic)
{
    if (!m_logger || m_logActive || m_logRequestPending)
        return;

    m_logRequestPath = path;
    m_pendingAutoLog = automatic;
    m_logRequestPending = true;

    if (!QMetaObject::invokeMethod(m_logger, "startLog", Qt::QueuedConnection,
                                   Q_ARG(QString, path), Q_ARG(bool, true))) {
        m_logRequestPending = false;
        showStatus(tr("无法调用日志线程"));
        return;
    }
    showStatus(tr("正在创建日志：%1").arg(path));
}

void MainWindow::endLog()
{
    if (!m_logger || !m_logThread || !m_logThread->isRunning())
        return;
    showStatus(tr("正在停止日志…"));
    QMetaObject::invokeMethod(m_logger, "stopLog", Qt::QueuedConnection);
}

void MainWindow::onLogStarted(const QString &path)
{
    m_logActive = true;
    m_logRequestPending = false;
    m_autoLogActive = m_pendingAutoLog;
    m_logPath = path;
    m_logBytes = 0;

    {
        QSignalBlocker blocker(m_logAction);
        m_logAction->setChecked(true);
    }
    updateLogLabel();
    m_receivePanel->appendSystemLine(tr("── 日志记录中：%1 ──").arg(path));
    showStatus(tr("日志已开始：%1").arg(path));
}

void MainWindow::onLogStopped()
{
    const bool wasLogging = m_logActive || m_logRequestPending;
    m_logActive = false;
    m_logRequestPending = false;
    m_autoLogActive = false;
    m_pendingAutoLog = false;

    {
        QSignalBlocker blocker(m_logAction);
        m_logAction->setChecked(false);
    }
    updateLogLabel();
    if (wasLogging) {
        m_receivePanel->appendSystemLine(tr("── 日志已停止 ──"));
        showStatus(tr("日志已停止"));
    }
}

void MainWindow::onLogError(const QString &message)
{
    m_logRequestPending = false;
    m_pendingAutoLog = false;
    m_receivePanel->appendSystemLine(tr("── 日志错误：%1 ──").arg(message));
    showStatus(message);
    QMessageBox::warning(this, tr("日志写入失败"), message);
}

void MainWindow::onLogOverflow(qint64 droppedBytes)
{
    m_receivePanel->appendSystemLine(
        tr("── 日志队列溢出，丢弃 %1 字节（文件中已插入缺口标记） ──").arg(droppedBytes));
    showStatus(tr("日志队列溢出，丢弃 %1 字节").arg(droppedBytes));
}

void MainWindow::onLogBytesWritten(qint64 totalBytes)
{
    m_logBytes = totalBytes;
    updateLogLabel();
}

void MainWindow::updateLogLabel()
{
    if (!m_logLabel)
        return;
    if (m_logActive) {
        m_logLabel->setText(tr("● 日志 %1").arg(formatBytes(m_logBytes)));
        m_logLabel->setToolTip(m_logPath);
        m_logLabel->setStyleSheet(QStringLiteral("color:#2EA043;font-weight:600;"));
    } else {
        m_logLabel->clear();
        m_logLabel->setToolTip(QString());
        m_logLabel->setStyleSheet(QString());
    }
}

void MainWindow::saveAutoReplyRules()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("autoReply"));
    s->setValue(QStringLiteral("rules"), m_autoReplyRules.toJson());
    s->setValue(QStringLiteral("enabled"), m_autoReplyEnabled);
    s->endGroup();
    SettingsManager::save();
}

void MainWindow::restoreAutoReplyRules()
{
    QSettings *s = SettingsManager::settings();
    s->beginGroup(QStringLiteral("autoReply"));
    const QString json = s->value(QStringLiteral("rules")).toString();
    m_autoReplyEnabled = s->value(QStringLiteral("enabled"), true).toBool();
    s->endGroup();

    if (!json.isEmpty())
        m_autoReplyRules.fromJson(json);

    if (m_autoReplyAction) {
        QSignalBlocker blocker(m_autoReplyAction);
        m_autoReplyAction->setChecked(m_autoReplyEnabled);
    }
}

QString MainWindow::makeLogPath(const QString &portName) const
{
    QString directory = SettingsManager::settings()
                            ->value(QStringLiteral("logging/dir"))
                            .toString();
    if (directory.isEmpty()) {
        QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        if (docs.isEmpty())
            docs = QDir::homePath();
        directory = docs + QStringLiteral("/SerialScopeLogs");
    }
    QDir().mkpath(directory);

    const QString base = QStringLiteral("serial_%1_%2")
                             .arg(sanitizedPortName(portName),
                                  QDateTime::currentDateTime().toString(
                                      QStringLiteral("yyyyMMdd_HHmmss")));
    QString path = QDir(directory).filePath(base + QStringLiteral(".log"));
    int suffix = 1;
    while (QFileInfo::exists(path)) {
        path = QDir(directory).filePath(
            base + QStringLiteral("_%1.log").arg(suffix++));
    }
    return path;
}

void MainWindow::showStatus(const QString &message)
{
    statusBar()->showMessage(message, 5000);
}

void MainWindow::toggleTheme(bool dark)
{
    Theme::apply(qobject_cast<QApplication *>(QCoreApplication::instance()), dark);
    SettingsManager::settings()->setValue(QStringLiteral("ui/dark"), dark);
    if (m_waveformPanel)
        m_waveformPanel->update();
    showStatus(dark ? tr("已切换到深色主题") : tr("已切换到浅色主题"));
}

void MainWindow::showCrcTool()
{
    CrcToolDialog dialog(this);
    dialog.exec();
}

void MainWindow::showEncodingTool()
{
    EncodingFixDialog dialog(this);
    dialog.exec();
}

void MainWindow::showAutoReplyRules()
{
    if (!m_autoReplyDialog) {
        m_autoReplyDialog = new AutoReplyDialog(this);
        connect(m_autoReplyDialog, &AutoReplyDialog::rulesChanged, this,
                [this](const AutoReplyEngine &rules) {
                    m_autoReplyRules = rules;
                    saveAutoReplyRules();
                });
    }

    m_autoReplyDialog->setRules(m_autoReplyRules);
    m_autoReplyDialog->exec();
}

void MainWindow::onAutoReplyToggled(bool enabled)
{
    m_autoReplyEnabled = enabled;
    saveAutoReplyRules();
    showStatus(enabled ? tr("自动回复已开启") : tr("自动回复已关闭"));
}

void MainWindow::showAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::refreshHotplug()
{
    if (!m_configPanel)
        return;
    m_configPanel->refreshPorts();
}

void MainWindow::shutdown()
{
    if (m_shuttingDown)
        return;
    m_shuttingDown = true;

    if (m_hotplugTimer)
        m_hotplugTimer->stop();
    if (m_rateTimer)
        m_rateTimer->stop();

    saveWindowState();

    if (m_worker && m_ioThread && m_ioThread->isRunning()) {
        QMetaObject::invokeMethod(m_worker, "closePort", Qt::QueuedConnection);
        m_ioThread->quit();
        m_ioThread->wait(3000);
    }

    if (m_logger && m_logThread && m_logThread->isRunning()) {
        QMetaObject::invokeMethod(m_logger, "stopLog", Qt::QueuedConnection);
        m_logThread->quit();
        m_logThread->wait(3000);
    }
}
