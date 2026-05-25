#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("slipgate");
    QCoreApplication::setOrganizationName("slipgate");

    MainWindow window;
    window.show();

    return app.exec();
}
