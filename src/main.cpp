#include <QApplication>
#include <QMessageBox>

#include "ui/MainWindow.h"
#include "MasterConfig.h"
#include "SetupDialog.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QString arg1 = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QString arg2 = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();
    bool lockMode = arg1 == "--lock";
    QString startupPath = lockMode ? QString() : arg1;

    if (lockMode) {
        if (!MasterConfig::exists()) {
            QMessageBox::critical(nullptr, "ByteLock",
                "Master Recovery is not set up yet.\n"
                "Open ByteLock normally to complete setup before locking folders.");
            return 1;
        }
        MainWindow window(startupPath, nullptr, arg2);
        return 0;
    }

    if (!MasterConfig::exists()) {
        SetupDialog setup;
        if (setup.exec() != QDialog::Accepted) {
            return 0;
        }
    }

    MasterConfig::repairOrphanedFolders();

    MainWindow window(startupPath, nullptr, QString());
    if (startupPath.isEmpty()) window.show();

    return app.exec();
}

