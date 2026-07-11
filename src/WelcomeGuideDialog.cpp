#include "WelcomeGuideDialog.h"
#include <QWizardPage>
#include <QVBoxLayout>
#include <QLabel>

namespace {
QWizardPage* makePage(const QString& title, const QString& body)
{
    auto* page = new QWizardPage;
    page->setTitle(title);
    auto* layout = new QVBoxLayout(page);
    auto* label = new QLabel(body, page);
    label->setWordWrap(true);
    layout->addWidget(label);
    return page;
}
}

WelcomeGuideDialog::WelcomeGuideDialog(QWidget* parent)
    : QWizard(parent)
{
    setWindowTitle("Welcome to ByteLock");
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(480, 340);

    addPage(makePage("Lock a Folder",
        "Right-click any folder in File Explorer and choose \"Lock with ByteLock\". "
        "Set a password. The folder is replaced by a hidden encrypted container and a "
        "visible .blocked placeholder file."));

    addPage(makePage("Unlock a Folder",
        "Double-click the .blocked file and enter your password to restore the folder."));

    addPage(makePage("Your Master Recovery Key",
        "You were just given a one-time Master Recovery Key. Save it somewhere safe "
        "(password manager, printed copy). It is the only way to recover a folder if "
        "you forget its password, and it can never be regenerated."));

    addPage(makePage("Uninstalling ByteLock",
        "If you ever uninstall ByteLock, you will be asked for your Master Recovery Key. "
        "All locked folders are automatically unlocked back to plain folders before removal."));

    setOption(QWizard::NoBackButtonOnStartPage, true);
    setButtonText(QWizard::FinishButton, "Got it");
}
