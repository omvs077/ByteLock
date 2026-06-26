#pragma once

#include <QWidget>

class QLineEdit;
class QToolButton;

class PasswordLineEdit : public QWidget
{
    Q_OBJECT

public:
    explicit PasswordLineEdit(QWidget* parent = nullptr);

    QString text() const;
    void setPlaceholderText(const QString& text);
    void clear();

signals:
    void textChanged(const QString& text);
    void returnPressed();

private:
    QLineEdit* m_lineEdit = nullptr;
    QToolButton* m_toggleButton = nullptr;
};
