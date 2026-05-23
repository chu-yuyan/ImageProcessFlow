#include "QtWidgetsApplication.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QtWidgetsApplication window;
    window.show();
    return app.exec();
}
