#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

struct TrackedFolder {
    QString originalPath;
    QString containerPath;
    bool locked = false;
    bool usesCustomPassword = false;
    QDateTime lastModified;
};

class FolderRegistry {
public:
    FolderRegistry();

    QVector<TrackedFolder> list() const;
    void upsert(const TrackedFolder& entry);
    void removeByOriginalPath(const QString& originalPath);
    bool findByAnyPath(const QString& path, TrackedFolder& outEntry) const;

private:
    QString m_iniPath;
};
