#pragma once

#include <QObject>
#include <QString>

#include <optional>

struct OperationResult {
    bool success = false;
    QString message;
};

class MainViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    explicit MainViewModel(QObject* parent = nullptr);
    ~MainViewModel() override;

    bool isBusy() const { return m_busy; }
    QString statusText() const { return m_statusText; }

    bool hasSessionPassword() const { return m_sessionPassword.has_value(); }

    Q_INVOKABLE QString opensslVersionText() const;

public slots:
    void clearSessionPassword() { m_sessionPassword.reset(); }

    void requestLockFolder(const QString& folderPath, const QString& password, bool rememberPassword);
    void requestUnlockFolder(const QString& containerPath, const QString& password, bool rememberPassword);

    void requestLockFolderWithSessionPassword(const QString& folderPath);
    void requestUnlockFolderWithSessionPassword(const QString& containerPath);

    void requestRunCryptoSelfTest();

signals:
    void busyChanged(bool busy);
    void statusTextChanged(const QString& text);
    void lockFinished(bool success, const QString& message);
    void unlockFinished(bool success, const QString& message);
    void selfTestFinished(bool success, const QString& message);

private:
    void setBusy(bool busy);
    void setStatusText(const QString& text);
    void runLock(const QString& folderPath, const QString& password);
    void runUnlock(const QString& containerPath, const QString& password);

    bool m_busy = false;
    QString m_statusText;
    std::optional<QString> m_sessionPassword;
};
