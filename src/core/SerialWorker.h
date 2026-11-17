#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QSerialPort>
#include <QString>
#include <QTimer>

#include "core/SerialConfig.h"

class QFile;

/// 串口 I/O 工作对象：在独立 QThread 中运行。
///
/// 设计要点：
///  - 接收数据先进入聚合缓冲，每 30ms 通过一次 dataReceived 信号批量上报，
///    避免高波特率下 UI 事件循环被逐字节信号打爆。
///  - 所有公开入口均为槽函数，由 UI 线程通过队列连接调用，天然线程安全。
class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);
    ~SerialWorker() override;

    bool isOpen() const;

    /// UI 线程调用：取走聚合后的接收数据（线程安全）。
    /// 多个 30ms 批次在 worker 侧合并为一次通知，UI 卡顿时不会堆积事件。
    QByteArray takeRxData(qint64 *droppedBytes = nullptr);

public slots:
    void openPort(const SerialConfig &cfg);
    void closePort();
    void sendData(const QByteArray &data);
    /// 在工作线程中分块读取并发送文件（带流控，避免一次性塞入巨大的写缓冲）
    void sendFile(const QString &path);
    void cancelFileSend();

signals:
    void portOpened(const QString &portName, qint32 baudRate);
    void portClosed();
    /// 有新的接收数据可取（仅通知，数据由 takeRxData() 拉取）
    void rxDataAvailable();
    void statsUpdated(qint64 txBytes, qint64 rxBytes);
    void errorOccurred(const QString &message);
    /// 已打开的端口因设备移除 / 资源错误而被动关闭
    void portLost(const QString &message);
    /// 写缓冲积压，本次发送被跳过（限流通知）
    void txBackpressure(qint64 queuedBytes);

    void fileSendProgress(qint64 sentBytes, qint64 totalBytes);
    void fileSendFinished(bool success, const QString &errorMessage);

private slots:
    void onReadyRead();
    void flushRxBuffer();
    void emitStats();
    void handleError(QSerialPort::SerialPortError error);
    void onBytesWritten(qint64 bytes);

private:
    void ensurePort();
    void closeInternal(bool emitSignal);
    void pumpFileWrite();
    void finishFileSend(bool success, const QString &message);
    void abortFileSend(const QString &message);

    QSerialPort *m_port = nullptr;
    QByteArray m_rxPending;
    QTimer m_flushTimer;   // 作为子对象创建，随 moveToThread 一起迁移线程
    QTimer m_statsTimer;
    qint64 m_txTotal = 0;
    qint64 m_rxTotal = 0;
    bool m_open = false;

    // 接收端有界合并队列（生产者：worker；消费者：UI）
    QMutex m_rxMutex;
    QByteArray m_rxShared;
    qint64 m_rxDroppedBytes = 0;
    bool m_rxNotifyPosted = false;
    qint64 m_lastBackpressureMs = 0;

    QFile *m_file = nullptr;
    qint64 m_fileTotal = 0;
    qint64 m_fileSent = 0;
    qint64 m_fileOutstanding = 0;
    QElapsedTimer m_fileProgressClock;
};
