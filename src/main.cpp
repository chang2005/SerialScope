#include "core/SerialConfig.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include "utils/SettingsManager.h"

#include <QApplication>
#include <QIcon>
#include <QSettings>

namespace {

QIcon loadAppIcon()
{
    QIcon icon;
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    for (const int size : sizes)
        icon.addFile(QStringLiteral(":/icons/app-%1.png").arg(size));
    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SerialScope"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("io.github.chang2005"));
    QCoreApplication::setApplicationName(QStringLiteral("SerialScope"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SERIALSCOPE_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("SerialScope"));
    qRegisterMetaType<SerialConfig>("SerialConfig");

    // 主题在创建窗口前应用，避免首帧闪烁
    const bool dark = QSettings().value(QStringLiteral("ui/dark"), false).toBool();
    Theme::apply(&app, dark);

    const QIcon appIcon = loadAppIcon();
    QApplication::setWindowIcon(appIcon);

    MainWindow window;
    window.setWindowIcon(appIcon);
    window.show();

    return app.exec();
}
