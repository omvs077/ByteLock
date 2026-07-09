#include <QApplication>

#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QString arg1 = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QString arg2 = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();
    bool lockMode = arg1 == "--lock";
    QString startupPath = lockMode ? QString() : arg1;
    MainWindow window(startupPath, nullptr, lockMode ? arg2 : QString());
    if (startupPath.isEmpty() && !lockMode) window.show();

    return app.exec();
}



