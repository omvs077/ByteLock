#pragma once
#include <QWizard>

class WelcomeGuideDialog : public QWizard
{
    Q_OBJECT
public:
    explicit WelcomeGuideDialog(QWidget* parent = nullptr);
};
