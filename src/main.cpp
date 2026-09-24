#include "core/appversion.h"
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName(QStringLiteral("tomoss"));
    a.setApplicationDisplayName(QStringLiteral("tomoss"));
    a.setApplicationVersion(QString::fromLatin1(version::semver()));
    a.setOrganizationName(QStringLiteral("toomoss"));

    MainWindow w;
    w.show();
    return QApplication::exec();
}
