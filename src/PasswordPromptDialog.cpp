#include "PasswordPromptDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>

PasswordPromptDialog::PasswordPromptDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("ByteLock");
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(12);

    m_label = new QLabel(this);
    m_label->setWordWrap(true);
    m_label->setStyleSheet("font-size: 13px; color: #1f2937;");
    layout->addWidget(m_label);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setEchoMode(QLineEdit::Password);
    m_lineEdit->setMinimumHeight(38);
    layout->addWidget(m_lineEdit);

    m_showCheckbox = new QCheckBox("Show password", this);
    m_showCheckbox->setStyleSheet("font-size: 12px; color: #4a5568;");
    connect(m_showCheckbox, &QCheckBox::toggled, this, [this](bool checked) {
        m_lineEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    layout->addWidget(m_showCheckbox);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    for (auto* btn : buttonBox->buttons()) btn->setMinimumHeight(40);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);

    connect(m_lineEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
}

void PasswordPromptDialog::setLabelText(const QString& text) { m_label->setText(text); }
void PasswordPromptDialog::setMaxLength(int len) { m_lineEdit->setMaxLength(len); }
QString PasswordPromptDialog::textValue() const { return m_lineEdit->text(); }
