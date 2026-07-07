#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QString startupPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    MainWindow window(startupPath);
    if (startupPath.isEmpty()) window.show();

    return app.exec();
}

