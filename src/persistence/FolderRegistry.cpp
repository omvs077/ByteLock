#include "persistence/FolderRegistry.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>

FolderRegistry::FolderRegistry()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    m_iniPath = dir + "/folders.ini";
}

QVector<TrackedFolder> FolderRegistry::list() const
{
    QSettings settings(m_iniPath, QSettings::IniFormat);
    QVector<TrackedFolder> result;

    int count = settings.beginReadArray("folders");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        TrackedFolder entry;
        entry.originalPath = settings.value("originalPath").toString();
        entry.containerPath = settings.value("containerPath").toString();
        entry.locked = settings.value("locked").toBool();
        entry.usesCustomPassword = settings.value("usesCustomPassword").toBool();
        entry.lastModified = settings.value("lastModified").toDateTime();
        result.append(entry);
    }
    settings.endArray();
    return result;
}

void FolderRegistry::upsert(const TrackedFolder& entry)
{
    QVector<TrackedFolder> entries = list();

    bool found = false;
    for (auto& existing : entries) {
        if (existing.originalPath == entry.originalPath) {
            existing = entry;
            found = true;
            break;
        }
    }
    if (!found) {
        entries.append(entry);
    }

    QSettings settings(m_iniPath, QSettings::IniFormat);
    settings.remove("folders");
    settings.beginWriteArray("folders");
    for (int i = 0; i < entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("originalPath", entries[i].originalPath);
        settings.setValue("containerPath", entries[i].containerPath);
        settings.setValue("locked", entries[i].locked);
        settings.setValue("usesCustomPassword", entries[i].usesCustomPassword);
        settings.setValue("lastModified", entries[i].lastModified);
    }
    settings.endArray();
}

void FolderRegistry::removeByOriginalPath(const QString& originalPath)
{
    QVector<TrackedFolder> entries = list();
    entries.removeIf([&](const TrackedFolder& e) { return e.originalPath == originalPath; });

    QSettings settings(m_iniPath, QSettings::IniFormat);
    settings.remove("folders");
    settings.beginWriteArray("folders");
    for (int i = 0; i < entries.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("originalPath", entries[i].originalPath);
        settings.setValue("containerPath", entries[i].containerPath);
        settings.setValue("locked", entries[i].locked);
        settings.setValue("usesCustomPassword", entries[i].usesCustomPassword);
        settings.setValue("lastModified", entries[i].lastModified);
    }
    settings.endArray();
}

bool FolderRegistry::findByAnyPath(const QString& path, TrackedFolder& outEntry) const
{
    for (const auto& e : list()) {
        if (e.originalPath == path || e.containerPath == path) {
            outEntry = e;
            return true;
        }
    }
    return false;
}
