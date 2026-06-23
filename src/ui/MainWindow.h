#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onCheckOpenSslClicked();

private:
    void setupUi();

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_checkButton = nullptr;
};
