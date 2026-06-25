#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget* parent = nullptr);

    void setMaximizedState(bool maximized);

signals:
    void minimizeRequested();
    void maximizeRestoreRequested();
    void closeRequested();

private:
    QLabel* m_titleLabel = nullptr;
    QPushButton* m_minimizeButton = nullptr;
    QPushButton* m_maximizeButton = nullptr;
    QPushButton* m_closeButton = nullptr;
};
