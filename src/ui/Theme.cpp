#include "ui/Theme.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleFactory>

bool Theme::s_dark = false;

void Theme::apply(QApplication *app, bool dark)
{
    s_dark = dark;
    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette palette;
    if (dark) {
        palette.setColor(QPalette::Window, QColor("#0D1117"));
        palette.setColor(QPalette::WindowText, QColor("#E6EDF3"));
        palette.setColor(QPalette::Base, QColor("#161B22"));
        palette.setColor(QPalette::AlternateBase, QColor("#1C2128"));
        palette.setColor(QPalette::ToolTipBase, QColor("#21262D"));
        palette.setColor(QPalette::ToolTipText, QColor("#E6EDF3"));
        palette.setColor(QPalette::Text, QColor("#E6EDF3"));
        palette.setColor(QPalette::PlaceholderText, QColor("#7D8590"));
        palette.setColor(QPalette::Button, QColor("#21262D"));
        palette.setColor(QPalette::ButtonText, QColor("#E6EDF3"));
        palette.setColor(QPalette::BrightText, QColor("#FFFFFF"));
        palette.setColor(QPalette::Link, QColor("#58A6FF"));
        palette.setColor(QPalette::Highlight, QColor("#2F81F7"));
        palette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#6E7681"));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#6E7681"));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#6E7681"));
        palette.setColor(QPalette::Disabled, QPalette::Base, QColor("#0D1117"));
        palette.setColor(QPalette::Disabled, QPalette::Button, QColor("#161B22"));
        palette.setColor(QPalette::Mid, QColor("#30363D"));
        palette.setColor(QPalette::Dark, QColor("#010409"));
    } else {
        palette.setColor(QPalette::Window, QColor("#F6F8FA"));
        palette.setColor(QPalette::WindowText, QColor("#1F2328"));
        palette.setColor(QPalette::Base, QColor("#FFFFFF"));
        palette.setColor(QPalette::AlternateBase, QColor("#F6F8FA"));
        palette.setColor(QPalette::ToolTipBase, QColor("#FFFFFF"));
        palette.setColor(QPalette::ToolTipText, QColor("#1F2328"));
        palette.setColor(QPalette::Text, QColor("#1F2328"));
        palette.setColor(QPalette::PlaceholderText, QColor("#8C959F"));
        palette.setColor(QPalette::Button, QColor("#F6F8FA"));
        palette.setColor(QPalette::ButtonText, QColor("#1F2328"));
        palette.setColor(QPalette::BrightText, QColor("#FFFFFF"));
        palette.setColor(QPalette::Link, QColor("#0969DA"));
        palette.setColor(QPalette::Highlight, QColor("#0969DA"));
        palette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#8C959F"));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#8C959F"));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#8C959F"));
        palette.setColor(QPalette::Mid, QColor("#D0D7DE"));
        palette.setColor(QPalette::Dark, QColor("#AFB8C1"));
    }

    app->setPalette(palette);
    app->setStyleSheet(styleSheet(dark));
}

bool Theme::isDark()
{
    return s_dark;
}

const QVector<QColor> &Theme::seriesColors()
{
    static const QVector<QColor> colors = {
        QColor("#2F81F7"), QColor("#F85149"), QColor("#3FB950"), QColor("#D29922"),
        QColor("#A371F7"), QColor("#39C5CF"), QColor("#DB6D28"), QColor("#E85AAD"),
        QColor("#7EE787"), QColor("#79C0FF"), QColor("#FFA657"), QColor("#FF7B72"),
    };
    return colors;
}

QColor Theme::rxColor()
{
    return s_dark ? QColor("#D6DEE8") : QColor("#24292F");
}

QColor Theme::txColor()
{
    return s_dark ? QColor("#56D364") : QColor("#1A7F37");
}

QColor Theme::secondaryTextColor()
{
    return s_dark ? QColor("#8B949E") : QColor("#57606A");
}

QColor Theme::filterHighlightColor()
{
    return s_dark ? QColor("#5A3E00") : QColor("#FFF3C4");
}

QColor Theme::filterHighlightTextColor()
{
    return s_dark ? QColor("#FFD866") : QColor("#7A4B00");
}

QFont Theme::monospaceFont(int pointSize)
{
    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(pointSize);
    font.setStyleHint(QFont::Monospace);
    return font;
}

QString Theme::styleSheet(bool dark)
{
    const QString window    = dark ? QStringLiteral("#0D1117") : QStringLiteral("#F6F8FA");
    const QString base      = dark ? QStringLiteral("#161B22") : QStringLiteral("#FFFFFF");
    const QString text      = dark ? QStringLiteral("#E6EDF3") : QStringLiteral("#1F2328");
    const QString muted     = dark ? QStringLiteral("#8B949E") : QStringLiteral("#57606A");
    const QString border    = dark ? QStringLiteral("#30363D") : QStringLiteral("#D0D7DE");
    const QString button    = dark ? QStringLiteral("#21262D") : QStringLiteral("#F6F8FA");
    const QString hover     = dark ? QStringLiteral("#30363D") : QStringLiteral("#EAEEF2");
    const QString accent    = dark ? QStringLiteral("#2F81F7") : QStringLiteral("#0969DA");
    const QString accentHov = dark ? QStringLiteral("#58A6FF") : QStringLiteral("#1F6FEB");
    const QString scroll    = dark ? QStringLiteral("#484F58") : QStringLiteral("#C6CDD5");

    QString qss = QStringLiteral(R"(
QMainWindow, QDialog { background: %WINDOW%; }
QWidget { color: %TEXT%; }
QToolBar { background: transparent; border: none; spacing: 6px; padding: 4px 6px; }
QToolBar::separator { background: %BORDER%; width: 1px; margin: 4px 6px; }
QToolButton {
    background: transparent; border: 1px solid transparent; border-radius: 6px;
    padding: 5px 10px;
}
QToolButton:hover { background: %HOVER%; }
QToolButton:checked { background: %BUTTON%; border: 1px solid %BORDER%; }
QToolButton:pressed { background: %HOVER%; }
QPushButton {
    background: %BUTTON%; border: 1px solid %BORDER%; border-radius: 6px;
    padding: 5px 14px; min-height: 22px;
}
QPushButton:hover { background: %HOVER%; }
QPushButton:pressed { background: %BORDER%; }
QPushButton:checked { background: %BUTTON%; border-color: %ACCENT%; }
QPushButton:disabled { color: %MUTED%; background: transparent; border-color: %BORDER%; }
QPushButton[primary="true"] {
    background: %ACCENT%; color: #FFFFFF; border: 1px solid %ACCENT%; font-weight: 600;
}
QPushButton[primary="true"]:hover { background: %ACCENT_HOVER%; border-color: %ACCENT_HOVER%; }
QPushButton[primary="true"]:disabled { background: %BUTTON%; color: %MUTED%; border-color: %BORDER%; }
QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit {
    background: %BASE%; border: 1px solid %BORDER%; border-radius: 6px;
    padding: 3px 8px; min-height: 22px;
    selection-background-color: %ACCENT%; selection-color: #FFFFFF;
}
QComboBox:hover, QSpinBox:hover, QLineEdit:hover { border-color: %ACCENT%; }
QComboBox:focus, QSpinBox:focus, QLineEdit:focus { border-color: %ACCENT%; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: %BASE%; border: 1px solid %BORDER%; selection-background-color: %ACCENT%;
    selection-color: #FFFFFF; outline: none; padding: 2px;
}
QPlainTextEdit, QTextEdit, QTextBrowser, QListWidget, QTableWidget {
    background: %BASE%; border: 1px solid %BORDER%; border-radius: 6px;
    selection-background-color: %ACCENT%; selection-color: #FFFFFF;
}
QPlainTextEdit:focus, QLineEdit:focus { border-color: %ACCENT%; }
QGroupBox {
    border: 1px solid %BORDER%; border-radius: 8px; margin-top: 12px;
    padding: 8px; background: transparent;
}
QGroupBox::title {
    subcontrol-origin: margin; left: 10px; padding: 0 4px; color: %MUTED%;
    font-weight: 600;
}
QCheckBox { spacing: 6px; }
QCheckBox:disabled { color: %MUTED%; }
QLabel[muted="true"] { color: %MUTED%; }
QLabel[error="true"] { color: #F85149; }
QStatusBar { background: transparent; border-top: 1px solid %BORDER%; }
QStatusBar QLabel { color: %MUTED%; padding: 0 6px; }
QSplitter::handle { background: %BORDER%; }
QSplitter::handle:horizontal { width: 4px; }
QSplitter::handle:vertical { height: 4px; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: %SCROLL%; min-height: 24px; border-radius: 5px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: %SCROLL%; min-width: 24px; border-radius: 5px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QHeaderView::section {
    background: %BUTTON%; border: none; border-bottom: 1px solid %BORDER%;
    padding: 4px 6px; color: %MUTED%;
}
QMenu { background: %BASE%; border: 1px solid %BORDER%; padding: 4px; }
QMenu::item { padding: 5px 22px; border-radius: 4px; }
QMenu::item:selected { background: %ACCENT%; color: #FFFFFF; }
QToolTip { background: %BASE%; color: %TEXT%; border: 1px solid %BORDER%; padding: 4px; }
)");
    qss.replace(QLatin1String("%WINDOW%"), window);
    qss.replace(QLatin1String("%BASE%"), base);
    qss.replace(QLatin1String("%TEXT%"), text);
    qss.replace(QLatin1String("%MUTED%"), muted);
    qss.replace(QLatin1String("%BORDER%"), border);
    qss.replace(QLatin1String("%BUTTON%"), button);
    qss.replace(QLatin1String("%HOVER%"), hover);
    qss.replace(QLatin1String("%ACCENT_HOVER%"), accentHov);
    qss.replace(QLatin1String("%ACCENT%"), accent);
    qss.replace(QLatin1String("%SCROLL%"), scroll);
    return qss;
}
