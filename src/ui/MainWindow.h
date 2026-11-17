#pragma once

#include "core/SerialConfig.h"

#include <QMainWindow>

class QAction;
class QLabel;
class QSplitter;
class QThread;
class QTimer;

class SerialConfigPanel;
class ReceivePanel;
class SendPanel;
class WaveformPanel;
class SerialWorker;
class DataLogger;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onOpenRequested(const SerialConfig &config);
    void onCloseRequested();
    void onPortOpened(const QString &portName, qint32 baudRate);
    void onPortClosed();
    void onPortLost(const QString &message);
    void onErrorOccurred(const QString &message);
    void onRxDataAvailable();
    void onStatsUpdated(qint64 txBytes, qint64 rxBytes);
    void onSendEcho(const QByteArray &data);

    void onLogActionToggled(bool enabled);
    void onAutoLogToggled(bool enabled);
    void onLogStarted(const QString &path);
    void onLogStopped();
    void onLogError(const QString &message);
    void onLogOverflow(qint64 droppedBytes);
    void onLogBytesWritten(qint64 totalBytes);

    void toggleTheme(bool dark);
    void showCrcTool();
    void showAbout();
    void updateRates();
    void refreshHotplug();
    void shutdown();

private:
    void setupUi();
    void setupToolbar();
    void setupThreads();
    void setupConnections();
    void restoreWindowState();
    void saveWindowState();
    void applyDefaultSplitterSizes();

    void handleIncomingData(const QByteArray &data);
    void updateConnectionStatus();
    void updateStatsLabels();
    void updateLogLabel();
    void beginLog(const QString &path, bool automatic);
    void endLog();
    QString makeLogPath(const QString &portName) const;
    QString configSummary(const SerialConfig &config) const;
    void showStatus(const QString &message);

    SerialConfigPanel *m_configPanel = nullptr;
    ReceivePanel *m_receivePanel = nullptr;
    SendPanel *m_sendPanel = nullptr;
    WaveformPanel *m_waveformPanel = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_leftSplitter = nullptr;

    QThread *m_ioThread = nullptr;
    SerialWorker *m_worker = nullptr;
    QThread *m_logThread = nullptr;
    DataLogger *m_logger = nullptr;

    QAction *m_logAction = nullptr;
    QAction *m_autoLogAction = nullptr;
    QAction *m_themeAction = nullptr;
    QAction *m_crcAction = nullptr;

    QLabel *m_connLabel = nullptr;
    QLabel *m_txLabel = nullptr;
    QLabel *m_rxLabel = nullptr;
    QLabel *m_rateLabel = nullptr;
    QLabel *m_logLabel = nullptr;

    QTimer *m_hotplugTimer = nullptr;
    QTimer *m_rateTimer = nullptr;

    SerialConfig m_currentConfig;
    bool m_connected = false;
    bool m_openPending = false;
    bool m_shuttingDown = false;
    bool m_hasSavedSplitterState = false;

    qint64 m_txTotal = 0;
    qint64 m_rxTotal = 0;
    qint64 m_lastTxSample = 0;
    qint64 m_lastRxSample = 0;
    double m_txRate = 0.0;
    double m_rxRate = 0.0;

    bool m_logActive = false;
    bool m_logRequestPending = false;
    bool m_autoLogActive = false;
    bool m_pendingAutoLog = false;
    QString m_logPath;
    qint64 m_logBytes = 0;
    QString m_logRequestPath;
};
