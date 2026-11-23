#pragma once

#include "core/SerialConfig.h"

#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QPushButton;
class QToolButton;

/// 顶部串口参数配置栏
class SerialConfigPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SerialConfigPanel(QWidget *parent = nullptr);
    ~SerialConfigPanel() override;

    void refreshPorts();
    void setConnected(bool connected);
    void setOpenPending(bool pending);
    bool isConnected() const { return m_connected; }

    SerialConfig currentConfig() const;
    void applyConfig(const SerialConfig &config);
    QString currentPortName() const;

    /// 当前控制线与自动重连状态（用于打开串口时同步给工作线程）
    bool dtrState() const;
    bool rtsState() const;
    bool autoReconnectEnabled() const;

signals:
    void openRequested(const SerialConfig &config);
    void closeRequested();
    /// DTR 电平变化
    void dtrChanged(bool on);
    /// RTS 电平变化
    void rtsChanged(bool on);
    /// 自动重连开关变化
    void autoReconnectChanged(bool enabled);

private slots:
    void onOpenClicked();
    void saveSettings();
    void commitBaud();

private:
    void setupUi();
    void restoreSettings();
    void setControlsEnabled(bool enabled);
    void updateOpenButtonState();
    void updatePortToolTip();
    QString selectedPortName() const;
    bool selectedBaudRate(qint32 *baudRate) const;
    bool validateFrameSettings(QString *error) const;
    int findPortRow(const QString &portName) const;

    QComboBox *m_portCombo = nullptr;
    QToolButton *m_refreshButton = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_dataBitsCombo = nullptr;
    QComboBox *m_parityCombo = nullptr;
    QComboBox *m_stopBitsCombo = nullptr;
    QComboBox *m_flowCombo = nullptr;
    QToolButton *m_dtrButton = nullptr;
    QToolButton *m_rtsButton = nullptr;
    QCheckBox *m_autoReconnectCheck = nullptr;
    QPushButton *m_openButton = nullptr;
    QStringList m_portSnapshot;
    qint32 m_committedBaud = 115200;
    bool m_portsInitialized = false;
    bool m_portUserEdited = false;
    bool m_connected = false;
    bool m_openPending = false;
};
