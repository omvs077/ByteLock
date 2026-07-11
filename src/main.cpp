#include <QApplication>
#include <QMessageBox>
#include <QIcon>

#include "ui/MainWindow.h"
#include "MasterConfig.h"
#include "engine/CryptoEngine.h"
#include "engine/FolderPacker.h"
#include "engine/SecureBytes.h"
#include <QInputDialog>
#include <QLineEdit>
#include <QFile>
#include "SetupDialog.h"
#include "WelcomeGuideDialog.h"

using namespace bytelock;

namespace {

const char* kGlobalStyle = R"(
    QDialog {
        background-color: #ffffff;
    }
    QLabel {
        color: #1f2937;
        font-size: 12px;
    }
    QLineEdit {
        border: 1px solid #d7dae0;
        border-radius: 6px;
        padding: 8px 10px;
        font-size: 12px;
        background-color: #f7f9fc;
    }
    QLineEdit:focus {
        border-color: #7aa7ff;
        background-color: #ffffff;
    }
    QPushButton {
        border: 1px solid #d7dae0;
        border-radius: 6px;
        padding: 7px 18px;
        font-size: 12px;
        background-color: #f7f9fc;
        min-width: 70px;
    }
    QPushButton:hover {
        background-color: #eaf1ff;
        border-color: #7aa7ff;
    }
    QPushButton:pressed {
        background-color: #dbe7ff;
    }
    QPushButton:default {
        background-color: #2f6fed;
        color: #ffffff;
        border-color: #2f6fed;
    }
    QPushButton:default:hover {
        background-color: #255dd1;
    }
    QMessageBox QLabel {
        font-size: 12px;
    }
)";

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyleSheet(kGlobalStyle);
    app.setWindowIcon(QIcon(":/icons/app.ico"));

    QString arg1 = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    QString arg2 = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QString();
    if (arg1 == "--verify-recovery") {
        bool ok = false;
        QString key = QInputDialog::getText(nullptr, "ByteLock Uninstall",
            "Enter your Master Recovery Key to continue uninstalling:",
            QLineEdit::Password, "", &ok);
        if (!ok || key.isEmpty() || !MasterConfig::verify(key)) return 1;
        return 0;
    }

    if (arg1 == "--unlock-all") {
        auto escrowKey = MasterConfig::getEscrowKey();
        if (escrowKey.empty()) return 1;
        for (const QString& folderPath : MasterConfig::getLockedFolders()) {
            QString containerPath = folderPath + ".blk";
            QString sidecarPath = folderPath + ".blk_recovery";
            QFile sidecar(sidecarPath);
            if (!sidecar.open(QIODevice::ReadOnly)) continue;
            QByteArray wrapped = sidecar.readAll();
            std::vector<uint8_t> wrappedBytes(wrapped.begin(), wrapped.end());
            auto unwrapped = CryptoEngine::decryptBuffer(wrappedBytes, escrowKey);
            if (!unwrapped) continue;
            SecureBytes folderKey(unwrapped.value().size());
            memcpy(folderKey.data(), unwrapped.value().data(), unwrapped.value().size());
            if (FolderPacker::unlockFolder(containerPath.toStdString(), folderPath.toStdString(), folderKey)) {
                QFile::remove(folderPath + ".blocked");
                MasterConfig::untrackLockedFolder(folderPath);
            }
        }
        return 0;
    }

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
        WelcomeGuideDialog guide;
        guide.exec();
    }

    MasterConfig::repairOrphanedFolders();

    MainWindow window(startupPath, nullptr, QString());
    if (startupPath.isEmpty()) window.show();

    return app.exec();
}
