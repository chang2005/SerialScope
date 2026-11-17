#pragma once

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QTimer>

/// 异步数据日志：appendRx/appendTx 线程安全，可在 UI 线程直接调用；
/// 文件写入在独立线程中每 250ms 批量执行。
///
/// 队列有界（16 MB），磁盘写入跟不上时丢弃最旧条目并发出 overflow 信号，
/// 保证串口接收链路永远不会因为日志而阻塞。
class DataLogger : public QObject
{
    Q_OBJECT
public:
    explicit DataLogger(QObject *parent = nullptr);
    ~DataLogger() override;

    void appendRx(const QByteArray &data);
    void appendTx(const QByteArray &data);
    bool isActive() const;

public slots:
    void startLog(const QString &path, bool includeTx = true);
    void stopLog();

signals:
    void started(const QString &path);
    void stopped();
    void errorOccurred(const QString &message);
    /// 队列溢出（限流到每秒最多一次），参数为本次丢弃字节数
    void overflow(qint64 droppedBytes);
    /// 已写入文件的累计字节数
    void bytesWritten(qint64 totalBytes);

private slots:
    void drain();

private:
    struct Entry {
        QDateTime timestamp;
        QByteArray data;
        bool tx = false;
    };

    void append(const QByteArray &data, bool tx);
    bool writeEntry(const Entry &entry);
    void failAndStop(const QString &message);

    mutable QMutex m_mutex;
    QQueue<Entry> m_queue;
    qint64 m_queuedBytes = 0;
    qint64 m_pendingGapBytes = 0;
    qint64 m_writtenTotal = 0;
    qint64 m_lastOverflowNotifyMs = 0;
    bool m_active = false;
    bool m_includeTx = true;

    QFile m_file;
    QTimer m_timer;
};
