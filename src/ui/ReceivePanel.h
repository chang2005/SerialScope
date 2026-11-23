#pragma once

#include "core/parser/ControlCharVisualizer.h"
#include "core/parser/HexCodec.h"
#include "core/parser/TextFilter.h"

#include <QWidget>

#include <memory>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QToolButton;

/// 接收显示区：ASCII / HEX 双视图，支持时间戳、自动滚动、暂停、
/// 显示发送回显、保存显示内容与 UTF-8 / GBK 增量解码。
class ReceivePanel : public QWidget
{
    Q_OBJECT
public:
    explicit ReceivePanel(QWidget *parent = nullptr);
    ~ReceivePanel() override;

    void appendRx(const QByteArray &data);
    void appendTx(const QByteArray &data);
    void appendSystemLine(const QString &text);

    bool showTx() const;
    void clear();

    void setPaused(bool paused);
    bool isPaused() const;

    bool isHexMode() const { return m_hexMode; }
    void setHexMode(bool hex);
    TextEncoding encoding() const;

    void saveDisplay();

signals:
    void showTxChanged(bool enabled);

private slots:
    void onModeChanged();
    void onEncodingChanged();
    void onPauseToggled(bool paused);
    void onClearClicked();
    void onSaveClicked();
    void onFilterChanged();
    void onFilterModeToggled(bool regex);
    void onShowControlCharsToggled(bool enabled);

private:
    void setupUi();
    void restoreSettings();
    void saveSettings();
    void appendData(const QByteArray &data, bool tx);
    void insertText(const QString &text, const QColor &color);
    void insertFilteredText(const QString &text, const QColor &color);
    void resetDecoder();
    QString chunkPrefix(bool tx) const;

    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_encodingCombo = nullptr;
    QCheckBox *m_timestampCheck = nullptr;
    QCheckBox *m_autoScrollCheck = nullptr;
    QCheckBox *m_showTxCheck = nullptr;
    QToolButton *m_pauseButton = nullptr;
    QPlainTextEdit *m_view = nullptr;

    // 过滤与不可见字符显示
    QLineEdit *m_filterEdit = nullptr;
    QToolButton *m_filterRegexButton = nullptr;
    QCheckBox *m_showControlCharsCheck = nullptr;
    TextFilter m_filter;
    ControlCharVisualizer m_controlChars;

    std::unique_ptr<QTextCodec::ConverterState> m_decodeState;
    bool m_hexMode = false;
    qint64 m_pausedDroppedBytes = 0;
};
