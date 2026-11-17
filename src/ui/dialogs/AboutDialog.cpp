#include "ui/dialogs/AboutDialog.h"

#include "ui/Theme.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr char kProjectUrl[] = "https://github.com/chang2005/SerialScope";
constexpr char kProjectUrlDisplay[] = "https://github.com/chang2005/SerialScope.git";

QLabel *mutedLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("muted", true);
    label->setWordWrap(true);
    return label;
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("关于 SerialScope"));
    setWindowIcon(QApplication::windowIcon());
    setModal(true);
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(26, 24, 26, 20);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout;
    header->setSpacing(18);

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QApplication::windowIcon().pixmap(72, 72));
    iconLabel->setFixedSize(72, 72);
    iconLabel->setScaledContents(true);
    header->addWidget(iconLabel, 0, Qt::AlignTop);

    auto *titleColumn = new QVBoxLayout;
    titleColumn->setSpacing(2);

    auto *title = new QLabel(QStringLiteral("SerialScope"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    titleColumn->addWidget(title);

    auto *version = new QLabel(
        tr("版本 V%1").arg(QCoreApplication::applicationVersion()), this);
    version->setProperty("muted", true);
    titleColumn->addWidget(version);

    auto *tagline = new QLabel(tr("跨平台串口调试与数据可视化工具"), this);
    titleColumn->addWidget(tagline);
    titleColumn->addStretch();
    header->addLayout(titleColumn, 1);
    layout->addLayout(header);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    auto *description = new QLabel(
        tr("支持 ASCII / HEX 收发、UTF-8 / GBK 编码、异步数据日志、CRC 校验、"
           "定时发送与文件发送；波形图可直接解析 ASCII 行内数值，也可按二进制帧"
           "协议提取多通道字段。"),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *githubRow = new QHBoxLayout;
    githubRow->setSpacing(8);
    githubRow->addWidget(new QLabel(tr("GitHub："), this));

    auto *githubLogo = new QToolButton(this);
    githubLogo->setAutoRaise(true);
    githubLogo->setCursor(Qt::PointingHandCursor);
    githubLogo->setIconSize(QSize(32, 32));
    githubLogo->setFixedSize(36, 36);
    githubLogo->setIcon(QIcon(Theme::isDark()
                                  ? QStringLiteral(":/icons/github-light.png")
                                  : QStringLiteral(":/icons/github-dark.png")));
    githubLogo->setToolTip(tr("打开 SerialScope GitHub 项目主页"));
    connect(githubLogo, &QToolButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QString::fromLatin1(kProjectUrl)));
    });
    githubRow->addWidget(githubLogo);

    auto *githubLink = new QLabel(
        QStringLiteral("<a href=\"%1\">%2</a>")
            .arg(QString::fromLatin1(kProjectUrl),
                 QString::fromLatin1(kProjectUrlDisplay)),
        this);
    githubLink->setTextFormat(Qt::RichText);
    githubLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
    githubLink->setOpenExternalLinks(true);
    githubLink->setToolTip(tr("在浏览器中打开项目主页"));
    githubRow->addWidget(githubLink, 1);
    layout->addLayout(githubRow);

    auto *licenseRow = new QHBoxLayout;
    licenseRow->setSpacing(8);
    licenseRow->addWidget(
        new QLabel(tr("许可证：GNU General Public License v3.0（GPL-3.0-only）"), this), 1);
    auto *licenseButton = new QPushButton(tr("查看许可证…"), this);
    connect(licenseButton, &QPushButton::clicked, this, &AboutDialog::showLicense);
    licenseRow->addWidget(licenseButton);
    layout->addLayout(licenseRow);

    layout->addWidget(mutedLabel(
        tr("Copyright © 2026 SerialScope Contributors · 基于 Qt %1 构建")
            .arg(QString::fromLatin1(qVersion())),
        this));

    layout->addSpacing(4);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    adjustSize();
    setFixedWidth(qMax(560, sizeHint().width()));
}

void AboutDialog::showLicense()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("GNU General Public License v3.0"));
    dialog.setWindowIcon(windowIcon());
    dialog.resize(760, 580);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *view = new QPlainTextEdit(&dialog);
    view->setReadOnly(true);
    view->setLineWrapMode(QPlainTextEdit::NoWrap);
    view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    QFile licenseFile(QStringLiteral(":/licenses/GPL-3.0.txt"));
    if (licenseFile.open(QIODevice::ReadOnly | QIODevice::Text))
        view->setPlainText(QString::fromUtf8(licenseFile.readAll()));
    else
        view->setPlainText(tr("未找到内置许可证文本，请查看程序目录中的 LICENSE 文件。"));
    layout->addWidget(view, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}
