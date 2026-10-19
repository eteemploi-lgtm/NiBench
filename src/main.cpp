#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("NiBench");
    QApplication::setOrganizationName("Acoustique");
    MainWindow w;
    w.resize(1100, 820);
    w.show();
    return app.exec();
}
