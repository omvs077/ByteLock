#pragma once
#include <QDialog>

class QLabel;
class QLineEdit;
class QCheckBox;

class PasswordPromptDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PasswordPromptDialog(QWidget* parent = nullptr);
    void setLabelText(const QString& text);
    void setMaxLength(int len);
    QString textValue() const;

private:
    QLabel* m_label;
    QLineEdit* m_lineEdit;
    QCheckBox* m_showCheckbox;
};
