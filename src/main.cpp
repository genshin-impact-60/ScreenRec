#include "appstyle.h"
#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

#ifndef SCREENREC_VERSION
#    define SCREENREC_VERSION "1.0.0"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ScreenRec"));
    QApplication::setOrganizationName(QStringLiteral("ScreenRec"));
    QApplication::setApplicationVersion(QStringLiteral(SCREENREC_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/app.png")));
    AppStyle::apply(app);

    MainWindow window;
    window.show();
    return QApplication::exec();
}
