#include "ui/PasswordLineEdit.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>

PasswordLineEdit::PasswordLineEdit(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setEchoMode(QLineEdit::Password);
    m_lineEdit->setMinimumHeight(36);

    m_toggleButton = new QToolButton(this);
    m_toggleButton->setCheckable(true);
    m_toggleButton->setText("Show");
    m_toggleButton->setCursor(Qt::PointingHandCursor);
    m_toggleButton->setMinimumHeight(36);

    layout->addWidget(m_lineEdit, 1);
    layout->addWidget(m_toggleButton);

    connect(m_toggleButton, &QToolButton::toggled, this, [this](bool checked) {
        m_lineEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        m_toggleButton->setText(checked ? "Hide" : "Show");
    });

    connect(m_lineEdit, &QLineEdit::textChanged, this, &PasswordLineEdit::textChanged);
    connect(m_lineEdit, &QLineEdit::returnPressed, this, &PasswordLineEdit::returnPressed);
}

QString PasswordLineEdit::text() const
{
    return m_lineEdit->text();
}

void PasswordLineEdit::setPlaceholderText(const QString& text)
{
    m_lineEdit->setPlaceholderText(text);
}

void PasswordLineEdit::clear()
{
    m_lineEdit->clear();
}
