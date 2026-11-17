#pragma once

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

    QStringList m_history;
    bool m_hexMode = false;
    bool m_portOpen = false;
    bool m_fileSending = false;
    int m_remainingSends = 0;
};
