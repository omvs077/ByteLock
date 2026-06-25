#pragma once

#include <QWidget>

class QButtonGroup;
class QPushButton;

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    enum class Page {
        Dashboard = 0,
        Settings = 1,
        About = 2
    };

    explicit Sidebar(QWidget* parent = nullptr);

    void setActivePage(Page page);

signals:
    void pageRequested(Page page);

private:
    QPushButton* m_dashboardButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QPushButton* m_aboutButton = nullptr;
    QButtonGroup* m_buttonGroup = nullptr;
};
