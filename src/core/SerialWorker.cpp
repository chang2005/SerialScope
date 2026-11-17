#include "core/SerialWorker.h"

#include <QDateTime>
#include <QFile>
#include <QMutexLocker>
#include <QSerialPortInfo>

namespace {
constexpr int kFlushIntervalMs = 30;      // 批量上报周期
constexpr int kStatsIntervalMs = 500;     // 统计周期
constexpr qint64 kMaxPendingBytes = 4 * 1024 * 1024; // 聚合缓冲硬上限

// UI 消费端有界队列：UI 卡顿（文件对话框等）时最多缓存 8 MB，超出丢弃最旧字节
constexpr qint64 kMaxSharedRxBytes = 8 * 1024 * 1024;
// 普通发送的写缓冲上限；超过时跳过本次发送，避免无界增长
constexpr qint64 kMaxTxQueueBytes = 1024 * 1024;

constexpr qint64 kFileChunkSize = 4096;
constexpr qint64 kFileHighWater = 32 * 1024; // 待写入高水位（字节）
constexpr int kFileProgressIntervalMs = 100;
}

SerialWorker::SerialWorker(QObject *parent)
    : QObject(parent)
    , m_flushTimer(this)
    , m_statsTimer(this)
{
    m_flushTimer.setInterval(kFlushIntervalMs);
    m_flushTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_flushTimer, &QTimer::timeout, this, &SerialWorker::flushRxBuffer);

    m_statsTimer.setInterval(kStatsIntervalMs);
    m_statsTimer.setTimerType(Qt::CoarseTimer);
    connect(&m_statsTimer, &QTimer::timeout, this, &SerialWorker::emitStats);
}

SerialWorker::~SerialWorker()
{
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    if (m_port) {
        closeInternal(false);
        delete m_port;
        m_port = nullptr;
    }
}

bool SerialWorker::isOpen() const
{
    return m_open;
}

void SerialWorker::ensurePort()
{
    if (m_port)
        return;
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead, this, &SerialWorker::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialWorker::handleError);
    connect(m_port, &QSerialPort::bytesWritten, this, &SerialWorker::onBytesWritten);
}

void SerialWorker::openPort(const SerialConfig &cfg)
{
    abortFileSend(tr("重新打开端口，文件发送已取消"));

    if (m_open)
        closeInternal(true);

    if (cfg.portName.trimmed().isEmpty()) {
        emit errorOccurred(tr("未选择串口端口"));
        return;
    }

    ensurePort();

    m_port->setPortName(cfg.portName);
    const bool settingsOk = m_port->setBaudRate(cfg.baudRate)
        && m_port->setDataBits(cfg.dataBits)
        && m_port->setParity(cfg.parity)
        && m_port->setStopBits(cfg.stopBits)
        && m_port->setFlowControl(cfg.flowControl);
    if (!settingsOk) {
        emit errorOccurred(tr("串口参数设置失败：%1").arg(m_port->errorString()));
        return;
    }

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("打开 %1 失败：%2")
                               .arg(cfg.portName, m_port->errorString()));
        return;
    }

    m_port->clear(QSerialPort::AllDirections);
    m_txTotal = 0;
    m_rxTotal = 0;
    m_rxPending.clear();
    {
        QMutexLocker locker(&m_rxMutex);
        m_rxShared.clear();
        m_rxDroppedBytes = 0;
        m_rxNotifyPosted = false;
    }
    m_open = true;

    m_flushTimer.start();
    m_statsTimer.start();

    emit portOpened(cfg.portName, cfg.baudRate);
    emit statsUpdated(m_txTotal, m_rxTotal);
}

void SerialWorker::closePort()
{
    closeInternal(true);
}

void SerialWorker::closeInternal(bool emitSignal)
{
    if (!m_port || !m_open)
        return;

    abortFileSend(tr("端口已关闭，文件发送已取消"));

    // 关闭前把最后一批数据交给 UI
    flushRxBuffer();

    m_flushTimer.stop();
    m_statsTimer.stop();
    m_open = false;
    m_port->close();
    m_rxPending.clear();

    if (emitSignal)
        emit portClosed();
}

void SerialWorker::onReadyRead()
{
    if (!m_port || !m_open)
        return;

    const QByteArray data = m_port->readAll();
    if (data.isEmpty())
        return;

    m_rxTotal += data.size();
    m_rxPending.append(data);

    // 极端情况下避免聚合缓冲无限增长
    if (m_rxPending.size() >= kMaxPendingBytes)
        flushRxBuffer();
}

void SerialWorker::flushRxBuffer()
{
    if (m_rxPending.isEmpty())
        return;

    QByteArray chunk;
    chunk.swap(m_rxPending);

    bool postNotification = false;
    {
        QMutexLocker locker(&m_rxMutex);
        m_rxShared.append(chunk);
        if (m_rxShared.size() > kMaxSharedRxBytes) {
            const int excess = static_cast<int>(m_rxShared.size() - kMaxSharedRxBytes);
            m_rxShared.remove(0, excess);
            m_rxDroppedBytes += excess;
        }
        if (!m_rxNotifyPosted) {
            m_rxNotifyPosted = true;
            postNotification = true;
        }
    }

    if (postNotification)
        emit rxDataAvailable();
}

QByteArray SerialWorker::takeRxData(qint64 *droppedBytes)
{
    QMutexLocker locker(&m_rxMutex);
    QByteArray data;
    data.swap(m_rxShared);
    if (droppedBytes)
        *droppedBytes = m_rxDroppedBytes;
    m_rxDroppedBytes = 0;
    m_rxNotifyPosted = false;
    return data;
}

void SerialWorker::emitStats()
{
    emit statsUpdated(m_txTotal, m_rxTotal);
}

void SerialWorker::sendData(const QByteArray &data)
{
    if (!m_port || !m_open || data.isEmpty())
        return;

    const qint64 queued = m_port->bytesToWrite();
    if (queued > kMaxTxQueueBytes) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastBackpressureMs > 1000) {
            m_lastBackpressureMs = now;
            emit txBackpressure(queued);
        }
        return;
    }

    const qint64 written = m_port->write(data);
    if (written < 0) {
        emit errorOccurred(tr("发送失败：%1").arg(m_port->errorString()));
        return;
    }
    m_txTotal += written;
    emit statsUpdated(m_txTotal, m_rxTotal);
}

void SerialWorker::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError
        || error == QSerialPort::NotOpenError) {
        return;
    }

    // open() 过程中的错误由 openPort() 负责报告，避免重复提示
    if (!m_open)
        return;

    const QString message = m_port ? m_port->errorString() : tr("未知串口错误");

    const bool fatal = error == QSerialPort::ResourceError
        || error == QSerialPort::DeviceNotFoundError
        || error == QSerialPort::PermissionError;

    if (fatal) {
        abortFileSend(tr("串口异常，文件发送已中止"));
        flushRxBuffer();
        m_open = false; // 防止 close() 触发 NotOpenError 重入
        m_flushTimer.stop();
        m_statsTimer.stop();
        m_rxPending.clear();
        if (m_port)
            m_port->close();
        emit portLost(message);
        emit portClosed();
    } else {
        emit errorOccurred(message);
    }
}

void SerialWorker::sendFile(const QString &path)
{
    abortFileSend(tr("文件发送已取消"));

    if (!m_open || !m_port) {
        emit fileSendFinished(false, tr("串口未打开"));
        return;
    }

    auto *file = new QFile(path, this);
    if (!file->open(QIODevice::ReadOnly)) {
        const QString error = file->errorString();
        file->deleteLater();
        emit fileSendFinished(false, tr("无法读取文件：%1").arg(error));
        return;
    }
    if (file->size() == 0) {
        file->close();
        file->deleteLater();
        emit fileSendFinished(false, tr("文件为空"));
        return;
    }

    m_file = file;
    m_fileTotal = file->size();
    m_fileSent = 0;
    m_fileOutstanding = 0;
    m_fileProgressClock.start();

    emit fileSendProgress(0, m_fileTotal);
    pumpFileWrite();
}

void SerialWorker::onBytesWritten(qint64 bytes)
{
    if (!m_file)
        return;
    m_fileOutstanding = qMax<qint64>(0, m_fileOutstanding - bytes);
    pumpFileWrite();
}

void SerialWorker::pumpFileWrite()
{
    if (!m_file || !m_port || !m_open)
        return;

    while (m_fileOutstanding < kFileHighWater && !m_file->atEnd()) {
        const QByteArray chunk = m_file->read(kFileChunkSize);
        if (chunk.isEmpty())
            break;
        const qint64 written = m_port->write(chunk);
        if (written != chunk.size()) {
            finishFileSend(false, tr("串口写入不完整：%1").arg(m_port->errorString()));
            return;
        }
        m_fileOutstanding += written;
        m_fileSent += written;
        m_txTotal += written;
    }

    if (m_file && m_file->atEnd() && m_fileOutstanding == 0) {
        finishFileSend(true, QString());
        return;
    }

    if (m_fileProgressClock.elapsed() >= kFileProgressIntervalMs) {
        m_fileProgressClock.restart();
        emit fileSendProgress(m_fileSent, m_fileTotal);
    }
}

void SerialWorker::finishFileSend(bool success, const QString &message)
{
    if (!m_file)
        return;

    const qint64 total = m_fileTotal;
    const qint64 sent = m_fileSent;

    m_file->close();
    m_file->deleteLater();
    m_file = nullptr;
    m_fileTotal = 0;
    m_fileSent = 0;
    m_fileOutstanding = 0;

    emit fileSendProgress(sent, total);
    emit fileSendFinished(success, message);
}

void SerialWorker::abortFileSend(const QString &message)
{
    if (m_file)
        finishFileSend(false, message);
}

void SerialWorker::cancelFileSend()
{
    abortFileSend(tr("文件发送已取消"));
}
