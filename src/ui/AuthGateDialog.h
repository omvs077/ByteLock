#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class PasswordLineEdit;

class AuthGateDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Mode {
        SetNewPassword,
        EnterPassword,
        SetCustomPassword,
        EnterCustomPassword
    };

    explicit AuthGateDialog(Mode mode, QWidget* parent = nullptr);

    QString verifiedPassword() const { return m_verifiedPassword; }

private slots:
    void onPrimaryButtonClicked();

private:
    void setupUi();

    Mode m_mode;
    QString m_verifiedPassword;

    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_errorLabel = nullptr;
    PasswordLineEdit* m_passwordField = nullptr;
    PasswordLineEdit* m_confirmField = nullptr;
    QPushButton* m_primaryButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
};
