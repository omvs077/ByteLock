#include "ui/AuthGateDialog.h"
#include "ui/PasswordLineEdit.h"
#include "engine/DefaultPasswordStore.h"
#include "engine/Result.h"

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace bytelock;

namespace {
const QString kOnboardingSubtitle =
    "This password protects every folder you lock by default.\n"
    "There is currently no recovery option if you forget it.";
const QString kLoginSubtitle = "Enter your password to continue.";
const QString kCustomSubtitle = "This password will only work for this folder.\nThe common password won't unlock it.";
const QString kEnterCustomSubtitle = "Enter the password set specifically for this folder.";
}

AuthGateDialog::AuthGateDialog(Mode mode, QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    const bool needsConfirmField = (m_mode == Mode::SetNewPassword || m_mode == Mode::SetCustomPassword);
    setFixedSize(380, needsConfirmField ? 380 : 280);
    setupUi();
}

void AuthGateDialog::setupUi()
{
    setObjectName("AuthGateDialog");
    setStyleSheet(R"(
        #AuthGateDialog {
            background-color: #FFFFFF;
            border: 1px solid #10193D;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 32, 32, 32);
    layout->setSpacing(12);

    QString titleText = "Enter Password";
    QString subtitleText = kLoginSubtitle;
    if (m_mode == Mode::SetNewPassword) {
        titleText = "Set Your Password";
        subtitleText = kOnboardingSubtitle;
    } else if (m_mode == Mode::SetCustomPassword) {
        titleText = "Set Custom Password";
        subtitleText = kCustomSubtitle;
    } else if (m_mode == Mode::EnterCustomPassword) {
        titleText = "Enter Custom Password";
        subtitleText = kEnterCustomSubtitle;
    }

    auto* titleLabel = new QLabel(titleText, this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: 700; color: #10193D;");

    m_subtitleLabel = new QLabel(subtitleText, this);
    m_subtitleLabel->setWordWrap(true);
    m_subtitleLabel->setStyleSheet("color: #8A93A6; font-size: 12px;");

    const bool isEnterMode = (m_mode == Mode::EnterPassword || m_mode == Mode::EnterCustomPassword);
    m_passwordField = new PasswordLineEdit(this);
    m_passwordField->setPlaceholderText(isEnterMode ? "Password" : "New password");

    layout->addWidget(titleLabel);
    layout->addWidget(m_subtitleLabel);
    layout->addWidget(m_passwordField);

    if (m_mode == Mode::SetNewPassword || m_mode == Mode::SetCustomPassword) {
        m_confirmField = new PasswordLineEdit(this);
        m_confirmField->setPlaceholderText("Confirm password");
        layout->addWidget(m_confirmField);
    }

    m_errorLabel = new QLabel("", this);
    m_errorLabel->setStyleSheet("color: #E5484D; font-size: 12px;");
    m_errorLabel->setWordWrap(true);
    layout->addWidget(m_errorLabel);

    layout->addStretch();

    m_primaryButton = new QPushButton(isEnterMode ? "Unlock" : "Set Password", this);
    m_primaryButton->setMinimumHeight(40);
    m_primaryButton->setCursor(Qt::PointingHandCursor);
    m_primaryButton->setStyleSheet(R"(
        QPushButton {
            background-color: #2F6FED;
            color: white;
            border: none;
            border-radius: 6px;
            font-weight: 600;
            font-size: 13px;
        }
        QPushButton:disabled {
            background-color: #C4CAD9;
        }
    )");
    layout->addWidget(m_primaryButton);

    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setMinimumHeight(32);
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #8A93A6;
            border: none;
            font-size: 12px;
        }
        QPushButton:hover {
            color: #10193D;
            text-decoration: underline;
        }
    )");
    layout->addWidget(m_cancelButton, 0, Qt::AlignHCenter);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_primaryButton, &QPushButton::clicked, this, &AuthGateDialog::onPrimaryButtonClicked);
    connect(m_passwordField, &PasswordLineEdit::returnPressed, this, &AuthGateDialog::onPrimaryButtonClicked);
}

void AuthGateDialog::onPrimaryButtonClicked()
{
    m_errorLabel->clear();
    const QString password = m_passwordField->text();

    if (password.isEmpty()) {
        m_errorLabel->setText("Password cannot be empty.");
        return;
    }

    if (m_mode == Mode::SetNewPassword || m_mode == Mode::SetCustomPassword) {
        if (password != m_confirmField->text()) {
            m_errorLabel->setText("Passwords do not match.");
            return;
        }
        if (password.length() < 8) {
            m_errorLabel->setText("Password must be at least 8 characters.");
            return;
        }
    }

    if (m_mode == Mode::SetCustomPassword || m_mode == Mode::EnterCustomPassword) {
        m_verifiedPassword = password;
        accept();
        return;
    }

    m_primaryButton->setEnabled(false);
    m_subtitleLabel->setText("Working, please wait...");

    std::string pwStd = password.toStdString();
    Mode mode = m_mode;

    auto* watcher = new QFutureWatcher<Result<bool>>(this);
    connect(watcher, &QFutureWatcher<Result<bool>>::finished, this, [this, watcher, password]() {
        Result<bool> result = watcher->result();
        watcher->deleteLater();
        m_primaryButton->setEnabled(true);
        m_subtitleLabel->setText(m_mode == Mode::SetNewPassword ? kOnboardingSubtitle : kLoginSubtitle);

        if (!result.isOk()) {
            m_errorLabel->setText(QString::fromStdString(result.errorMessage()));
            return;
        }

        if (result.value()) {
            m_verifiedPassword = password;
            accept();
        } else {
            m_errorLabel->setText("Incorrect password. Please try again.");
            m_passwordField->clear();
        }
    });

    QFuture<Result<bool>> future = QtConcurrent::run([pwStd, mode]() -> Result<bool> {
        DefaultPasswordStore store;
        if (mode == Mode::SetNewPassword) {
            auto createResult = store.createNew(pwStd);
            if (!createResult) {
                return Result<bool>::fail(createResult.error(), createResult.detail());
            }
            return Result<bool>::ok(true);
        }
        return store.verify(pwStd);
    });

    watcher->setFuture(future);
}
