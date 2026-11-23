#pragma once

#include "core/QuickSend.h"
#include "core/SequenceSender.h"

#include <QByteArray>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class QToolButton;

/// 发送区：ASCII / HEX 双模式、行尾、校验、定时发送、历史记录、文件发送。
class SendPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SendPanel(QWidget *parent = nullptr);
    ~SendPanel() override;

    void setPortOpen(bool open);
    bool isHexMode() const { return m_hexMode; }

public slots:
    void setFileProgress(qint64 sentBytes, qint64 totalBytes);
    void onFileSendFinished(bool success, const QString &errorMessage);
    void onTxBackpressure(qint64 queuedBytes);
    /// 收到数据时通知发送区：应答触发模式下据此发送下一条
    void onDataReceived();

signals:
    void dataToSend(const QByteArray &data);
    void fileSendRequested(const QString &path);
    void cancelFileSendRequested();
    void statusMessage(const QString &message);

private slots:
    void onSendClicked();
    void onModeChanged();
    void onTimedToggled(bool enabled);
    void onTimedTimeout();
    void onHistoryActivated(int index);
    void onFileClicked();
    void onTextChanged();
    void onIntervalChanged(int value);
    void onConvertClicked();
    void onQuickSendClicked();
    void onQuickSendSave();
    void onQuickSendDelete();
    void rebuildQuickSendButtons();
    void onSequenceEdit();
    void onSequenceStart();
    void onSequenceStop();
    void onSequenceModeChanged(int index);

private:
    void setupUi();
    void restoreSettings();
    void saveSettings();
    void updateControls();
    void setHint(const QString &message, bool isError = true);
    bool validateInput(QString *error) const;
    bool buildPayload(QByteArray *payload, QString *error) const;
    bool sendCurrent(bool addToHistory);
    void addHistory(const QString &text);
    void rebuildHistoryCombo();
    /// 按当前模式构建负载（不含行尾与校验），供互转与快捷发送复用
    bool buildRawPayload(QByteArray *payload, QString *error) const;
    /// 用当前输入内容构造一条快捷发送条目
    QuickSendItem currentQuickSendItem() const;
    /// 发送轮发序列的当前条目；应答触发模式下会等待应答
    void sendNextSequenceItem();
    /// 启动轮发（定时或应答触发）
    bool startSequence();
    /// 停止轮发
    void stopSequence();
    /// 刷新轮发状态显示
    void updateSequenceStatus();

    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_encodingCombo = nullptr;
    QComboBox *m_lineEndingCombo = nullptr;
    QComboBox *m_checksumCombo = nullptr;
    QPlainTextEdit *m_input = nullptr;
    QComboBox *m_historyCombo = nullptr;
    QCheckBox *m_timedCheck = nullptr;
    QSpinBox *m_intervalSpin = nullptr;
    QSpinBox *m_countSpin = nullptr;
    QPushButton *m_sendButton = nullptr;
    QPushButton *m_fileButton = nullptr;
    QLabel *m_hintLabel = nullptr;
    QTimer *m_timedTimer = nullptr;
    QString m_fileName;

    // ASCII / HEX 互转
    QToolButton *m_convertButton = nullptr;

    // 快捷发送
    QWidget *m_quickSendBar = nullptr;
    QToolButton *m_quickSendSaveButton = nullptr;
    QToolButton *m_quickSendDeleteButton = nullptr;
    QVector<QPushButton *> m_quickSendButtons;
    QuickSendList m_quickSends;

    // 轮发序列
    QWidget *m_sequenceBar = nullptr;
    QComboBox *m_sequenceModeCombo = nullptr;
    QToolButton *m_sequenceEditButton = nullptr;
    QToolButton *m_sequenceStartButton = nullptr;
    QLabel *m_sequenceStatusLabel = nullptr;
    SequenceSet m_sequence;
    SequenceRunner m_sequenceRunner;
    QTimer *m_sequenceTimer = nullptr;
    bool m_sequenceRunning = false;
    bool m_sequenceAwaitingReply = false;

    QStringList m_history;
    bool m_hexMode = false;
    bool m_portOpen = false;
    bool m_fileSending = false;
    int m_remainingSends = 0;
};
