#include "ui/SettingsPage.h"

#include <QLabel>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    auto* label = new QLabel("Settings - coming in Phase 4B-5", this);
    label->setAlignment(Qt::AlignCenter);
    layout->addStretch();
    layout->addWidget(label);
    layout->addStretch();
}
