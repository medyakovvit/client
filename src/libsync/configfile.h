/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "owncloudlib.h"
#include <memory>
#include <QSharedPointer>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <chrono>
#include "common/result.h"

class QWidget;
class QHeaderView;
class ExcludedFiles;

namespace OCC {

class AbstractCredentials;

/**
 * @brief The ConfigFile class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT ConfigFile
{
public:
    static QString configPath();
    static QString configFile();
    static bool exists();

    ConfigFile();

    enum Scope { UserScope,
        SystemScope };

    QString excludeFile(Scope scope) const;
    static QString excludeFileFromSystem(); // doesn't access config dir

    /**
     * Creates a backup of the file
     *
     * Returns the path of the new backup.
     */
    QString backup() const;


    QString defaultConnection() const;

    bool passwordStorageAllowed(const QString &connection = QString());

    /* Server poll interval in milliseconds */
    std::chrono::milliseconds remotePollInterval(std::chrono::seconds defaultVal, const QString &connection = QString()) const;
    /* Set poll interval. Value in milliseconds has to be larger than 5000 */
    void setRemotePollInterval(std::chrono::milliseconds interval, const QString &connection = QString());

    /* Interval to check for new notifications */
    std::chrono::milliseconds notificationRefreshInterval(const QString &connection = QString()) const;

    /* Force sync interval, in milliseconds */
    std::chrono::milliseconds forceSyncInterval(std::chrono::seconds remoteFromCapabilities, const QString &connection = QString()) const;

    /**
     * Interval in milliseconds within which full local discovery is required
     *
     * Use -1 to disable regular full local discoveries.
     */
    std::chrono::milliseconds fullLocalDiscoveryInterval() const;

    bool monoIcons() const;
    void setMonoIcons(bool);

    bool promptDeleteFiles() const;
    void setPromptDeleteFiles(bool promptDeleteFiles);

    bool crashReporter() const;
    void setCrashReporter(bool enabled);

    /** Whether to set up logging to a temp directory on startup.
     *
     * Configured via the log window. Not used if command line sets up logging.
     */
    bool automaticLogDir() const;
    void setAutomaticLogDir(bool enabled);

    /** Wheter the automaticLogDir should expire logs, and after how many hours */
    Optional<std::chrono::hours> automaticDeleteOldLogsAge() const;
    void setAutomaticDeleteOldLogsAge(Optional<std::chrono::hours> expireTime);

    /** Whether to log http traffic */
    void setLogHttp(bool b);
    bool logHttp() const;

    // Whether experimental UI options should be shown
    bool showExperimentalOptions() const;

    // proxy settings
    void setProxyType(int proxyType,
        const QString &host = QString(),
        int port = 0, bool needsAuth = false,
        const QString &user = QString(),
        const QString &pass = QString());

    int proxyType() const;
    QString proxyHostName() const;
    int proxyPort() const;
    bool proxyNeedsAuth() const;
    QString proxyUser() const;
    QString proxyPassword() const;

    /** 0: no limit, 1: manual, >0: automatic */
    int useUploadLimit() const;
    int useDownloadLimit() const;
    void setUseUploadLimit(int);
    void setUseDownloadLimit(int);
    /** in kbyte/s */
    int uploadLimit() const;
    int downloadLimit() const;
    void setUploadLimit(int kbytes);
    void setDownloadLimit(int kbytes);
    /** [checked, size in MB] **/
    QPair<bool, qint64> newBigFolderSizeLimit() const;
    void setNewBigFolderSizeLimit(bool isChecked, qint64 mbytes);
    bool confirmExternalStorage() const;
    void setConfirmExternalStorage(bool);

    /** If we should move the files deleted on the server in the trash  */
    bool moveToTrash() const;
    void setMoveToTrash(bool);

    static bool setConfDir(const QString &value);

    bool optionalDesktopNotifications() const;
    void setOptionalDesktopNotifications(bool show);

    bool showInExplorerNavigationPane() const;
    void setShowInExplorerNavigationPane(bool show);

    int timeout() const;
    qint64 chunkSize() const;
    qint64 maxChunkSize() const;
    qint64 minChunkSize() const;
    std::chrono::milliseconds targetChunkUploadDuration() const;

    void saveGeometry(QWidget *w);
    void restoreGeometry(QWidget *w);

    // how often the check about new versions runs
    std::chrono::milliseconds updateCheckInterval(const QString &connection = QString()) const;

    bool skipUpdateCheck(const QString &connection = QString()) const;
    void setSkipUpdateCheck(bool, const QString &);

    QString updateChannel() const;
    void setUpdateChannel(const QString &channel);

    QString uiLanguage() const;
    void setUiLanguage(const QString &uiLanguage);

    void saveGeometryHeader(QHeaderView *header);
    void restoreGeometryHeader(QHeaderView *header);

    /** The client version that last used this settings file.
        Updated by configVersionMigration() at client startup. */
    QString clientVersionString() const;
    void setClientVersionString(const QString &version);

    /**  Returns a new settings pre-set in a specific group. */
    static std::unique_ptr<QSettings> settingsWithGroup(const QString &group);

    /// Add the system and user exclude file path to the ExcludedFiles instance.
    static void setupDefaultExcludeFilePaths(ExcludedFiles &excludedFiles);

protected:
    QVariant getPolicySetting(const QString &policy, const QVariant &defaultValue = QVariant()) const;
    void storeData(const QString &group, const QString &key, const QVariant &value);
    void removeData(const QString &group, const QString &key);
    bool dataExists(const QString &group, const QString &key) const;

private:
    QVariant getValue(const QString &param, const QString &group = QString(),
        const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

private:
    typedef QSharedPointer<AbstractCredentials> SharedCreds;

    static QString _oCVersion;
    static QString _confDir;
};
}
#endif // CONFIGFILE_H
