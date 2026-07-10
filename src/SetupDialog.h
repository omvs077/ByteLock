#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;
class QCheckBox;
class QPushButton;

class SetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit SetupDialog(QWidget* parent = nullptr);

private slots:
    void onCopyClicked();
    void onSaveClicked();
    void onCheckboxToggled(bool checked);

private:
    QString m_recoveryKey;
    QLineEdit* m_keyField;
    QCheckBox* m_confirmCheck;
    QPushButton* m_okButton;
};
