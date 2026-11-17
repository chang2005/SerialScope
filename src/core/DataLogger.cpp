#include "core/DataLogger.h"

#include "core/parser/HexCodec.h"

#include <QMutexLocker>

namespace {
constexpr qint64 kMaxQueuedBytes = 16LL * 1024 * 1024; // 16 MB
constexpr int kDrainIntervalMs = 250;
constexpr int kBytesPerLogLine = 32;
}

DataLogger::DataLogger(QObject *parent)
    : QObject(parent)
    , m_timer(this)
{
    m_timer.setInterval(kDrainIntervalMs);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &DataLogger::drain);
}

DataLogger::~DataLogger()
{
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
}

bool DataLogger::isActive() const
{
    QMutexLocker locker(&m_mutex);
    return m_active;
}

void DataLogger::appendRx(const QByteArray &data)
{
    append(data, false);
}

void DataLogger::appendTx(const QByteArray &data)
{
    append(data, true);
}

void DataLogger::append(const QByteArray &data, bool tx)
{
    if (data.isEmpty())
        return;

    qint64 notifyDropped = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_active)
            return;
        if (tx && !m_includeTx)
            return;

        Entry entry;
        entry.timestamp = QDateTime::currentDateTime();
        entry.data = data;
        entry.tx = tx;
        m_queue.enqueue(entry);
        m_queuedBytes += data.size();

        qint64 dropped = 0;
        while (m_queuedBytes > kMaxQueuedBytes && m_queue.size() > 1) {
            const Entry old = m_queue.dequeue();
            m_queuedBytes -= old.data.size();
            dropped += old.data.size();
        }
        if (dropped > 0) {
            m_pendingGapBytes += dropped;
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            if (now - m_lastOverflowNotifyMs >= 1000) {
                m_lastOverflowNotifyMs = now;
                notifyDropped = dropped;
            }
        }
    }

    if (notifyDropped > 0)
        emit overflow(notifyDropped);
}

void DataLogger::startLog(const QString &path, bool includeTx)
{
    if (isActive())
        stopLog(); // 关闭旧文件并落盘

    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(tr("无法创建日志文件：%1").arg(m_file.errorString()));
        return;
    }

    const QString header = QStringLiteral("# SerialScope log %1\n")
                               .arg(QDateTime::currentDateTime()
                                        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    m_file.write(header.toUtf8());
    m_file.flush();
    m_writtenTotal = header.toUtf8().size();

    {
        QMutexLocker locker(&m_mutex);
        m_includeTx = includeTx;
        m_active = true;
        m_pendingGapBytes = 0;
    }

    m_timer.start();
    emit bytesWritten(m_writtenTotal);
    emit started(path);
}

void DataLogger::stopLog()
{
    bool wasActive = false;
    {
        QMutexLocker locker(&m_mutex);
        wasActive = m_active;
        m_active = false;
    }

    drain();
    m_timer.stop();
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }

    if (wasActive)
        emit stopped();
}

void DataLogger::drain()
{
    QQueue<Entry> batch;
    qint64 gapBytes = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (m_queue.isEmpty() && m_pendingGapBytes == 0)
            return;
        batch.swap(m_queue);
        m_queuedBytes = 0;
        gapBytes = m_pendingGapBytes;
        m_pendingGapBytes = 0;
    }

    if (gapBytes > 0 && m_file.isOpen()) {
        const QByteArray marker = QStringLiteral("# 丢弃 %1 字节 @ %2\n")
                                      .arg(gapBytes)
                                      .arg(QDateTime::currentDateTime().toString(
                                          QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
                                      .toUtf8();
        if (m_file.write(marker) != marker.size()) {
            failAndStop(tr("写入日志失败：%1").arg(m_file.errorString()));
            return;
        }
        m_writtenTotal += marker.size();
    }

    const qint64 before = m_writtenTotal;
    while (!batch.isEmpty()) {
        const Entry entry = batch.dequeue();
        if (!writeEntry(entry)) {
            failAndStop(tr("写入日志失败：%1").arg(m_file.errorString()));
            return;
        }
    }
    if (m_file.isOpen())
        m_file.flush();
    if (m_writtenTotal != before)
        emit bytesWritten(m_writtenTotal);
}

bool DataLogger::writeEntry(const Entry &entry)
{
    if (!m_file.isOpen())
        return false;

    const QString stamp = entry.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString firstPrefix = QStringLiteral("[%1] %2 ")
                                    .arg(stamp, entry.tx ? QStringLiteral("TX")
                                                         : QStringLiteral("RX"));
    const QByteArray indent(firstPrefix.size(), ' ');

    bool first = true;
    for (int offset = 0; offset < entry.data.size(); offset += kBytesPerLogLine) {
        const QByteArray block = entry.data.mid(offset, kBytesPerLogLine);
        QByteArray line;
        if (first)
            line = firstPrefix.toUtf8();
        else
            line = indent;
        line += HexCodec::formatHex(block).toUtf8();
        line += '\n';
        if (m_file.write(line) != line.size())
            return false;
        m_writtenTotal += line.size();
        first = false;
    }
    return true;
}

void DataLogger::failAndStop(const QString &message)
{
    {
        QMutexLocker locker(&m_mutex);
        m_active = false;
        m_queue.clear();
        m_queuedBytes = 0;
    }
    m_timer.stop();
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
    emit errorOccurred(message);
    emit stopped();
}
