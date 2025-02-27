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

#include "accessmanager.h"
#include "application.h"
#include "common/utility.h"
#include "common/version.h"
#include "configfile.h"
#include "theme.h"

#include "updater/ocupdater.h"

#include <QObject>
#include <QtCore>
#include <QtNetwork>
#include <QtGui>
#include <QtWidgets>

#include <stdio.h>

namespace OCC {

static const char updateAvailableC[] = "Updater/updateAvailable";
static const char updateTargetVersionC[] = "Updater/updateTargetVersion";
static const char updateTargetVersionStringC[] = "Updater/updateTargetVersionString";
static const char seenVersionC[] = "Updater/seenVersion";
static const char autoUpdateAttemptedC[] = "Updater/autoUpdateAttempted";


UpdaterScheduler::UpdaterScheduler(QObject *parent)
    : QObject(parent)
{
    connect(&_updateCheckTimer, &QTimer::timeout,
        this, &UpdaterScheduler::slotTimerFired);

    // Note: the sparkle-updater is not an OCUpdater
    if (OCUpdater *updater = qobject_cast<OCUpdater *>(Updater::instance())) {
        connect(updater, &OCUpdater::newUpdateAvailable,
            this, &UpdaterScheduler::updaterAnnouncement);
        connect(updater, &OCUpdater::requestRestart, this, &UpdaterScheduler::requestRestart);
    }

    // at startup, do a check in any case.
    QTimer::singleShot(3000, this, &UpdaterScheduler::slotTimerFired);

    ConfigFile cfg;
    auto checkInterval = cfg.updateCheckInterval();
    _updateCheckTimer.start(std::chrono::milliseconds(checkInterval).count());
}

void UpdaterScheduler::slotTimerFired()
{
    ConfigFile cfg;

    // re-set the check interval if it changed in the config file meanwhile
    auto checkInterval = std::chrono::milliseconds(cfg.updateCheckInterval()).count();
    if (checkInterval != _updateCheckTimer.interval()) {
        _updateCheckTimer.setInterval(checkInterval);
        qCInfo(lcUpdater) << "Setting new update check interval " << checkInterval;
    }

    // consider the skipUpdateCheck flag in the config.
    if (cfg.skipUpdateCheck()) {
        qCInfo(lcUpdater) << "Skipping update check because of config file";
        return;
    }

    Updater *updater = Updater::instance();
    if (updater) {
        updater->backgroundCheckForUpdate();
    }
}


/* ----------------------------------------------------------------- */

OCUpdater::OCUpdater(const QUrl &url)
    : Updater()
    , _updateUrl(url)
    , _state(Unknown)
    , _accessManager(new AccessManager(this))
    , _timeoutWatchdog(new QTimer(this))
{
}

void OCUpdater::setUpdateUrl(const QUrl &url)
{
    _updateUrl = url;
}

bool OCUpdater::performUpdate()
{
    QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    if (!updateFile.isEmpty() && QFile(updateFile).exists()
        && !updateSucceeded() /* Someone might have run the updater manually between restarts */) {
        const QString name = Theme::instance()->appNameGUI();
        if (QMessageBox::information(nullptr, tr("New %1 Update Ready").arg(name),
                tr("A new update for %1 is about to be installed. The updater may ask\n"
                   "for additional privileges during the process.")
                    .arg(name),
                QMessageBox::Ok)) {
            slotStartInstaller();
            return true;
        }
    }
    return false;
}

void OCUpdater::backgroundCheckForUpdate()
{
    int dlState = downloadState();

    // do the real update check depending on the internal state of updater.
    switch (dlState) {
    case Unknown:
    case UpToDate:
    case DownloadFailed:
    case DownloadTimedOut:
        qCInfo(lcUpdater) << "Checking for available update";
        checkForUpdate();
        break;
    case DownloadComplete:
        qCInfo(lcUpdater) << "Update is downloaded, skip new check.";
        break;
    case UpdateOnlyAvailableThroughSystem:
        qCInfo(lcUpdater) << "Update is only available through system, skip check.";
        break;
    }
}

QString OCUpdater::statusString() const
{
    QString updateVersion = _updateInfo.versionString();

    switch (downloadState()) {
    case Downloading:
        return tr("Downloading %1. Please wait...").arg(updateVersion);
    case DownloadComplete:
        return tr("%1 available. Restart application to start the update.").arg(updateVersion);
    case DownloadFailed:
        return tr("Could not download update. Please click <a href='%1'>here</a> to download the update manually.").arg(_updateInfo.web());
    case DownloadTimedOut:
        return tr("Could not check for new updates.");
    case UpdateOnlyAvailableThroughSystem:
#ifdef Q_OS_LINUX
        // https://docs.appimage.org/packaging-guide/environment-variables.html
        // TODO: update once AppImageUpdate has been implemented
        if (qEnvironmentVariableIsSet("APPIMAGE")) {
            return tr("New %1 available. Please click <a href='%2'>here</a> to download the new AppImage manually.").arg(updateVersion, _updateInfo.web());
        }
#endif
        return tr("New %1 available. Please use the system's update tool to install it.").arg(updateVersion);
    case CheckingServer:
        return tr("Checking update server...");
    case Unknown:
        return tr("Update status is unknown: Did not check for new updates.");
    case UpToDate:
    // fall through
    default:
        return tr("No updates available. Your installation is at the latest version.");
    }
}

int OCUpdater::downloadState() const
{
    return _state;
}

void OCUpdater::setDownloadState(DownloadState state)
{
    auto oldState = _state;
    _state = state;
    emit downloadStateChanged();

    // show the notification if the download is complete (on every check)
    // or once for system based updates.
    if (_state == OCUpdater::DownloadComplete || (oldState != OCUpdater::UpdateOnlyAvailableThroughSystem
                                                     && _state == OCUpdater::UpdateOnlyAvailableThroughSystem)) {
        emit newUpdateAvailable(tr("Update Check"), statusString());
    }
}

void OCUpdater::slotStartInstaller()
{
    QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);
    QString updateFile = settings.value(updateAvailableC).toString();
    settings.setValue(autoUpdateAttemptedC, true);
    settings.sync();
    qCInfo(lcUpdater) << "Running updater" << updateFile;

    if(updateFile.endsWith(".exe")) {
        QProcess::startDetached(updateFile, QStringList() << "/S"
                                                          << "/launch");
    } else if(updateFile.endsWith(".msi")) {
        // When MSIs are installed without gui they cannot launch applications
        // as they lack the user context. That is why we need to run the client
        // manually here. We wrap the msiexec and client invocation in a powershell
        // script because owncloud.exe will be shut down for installation.
        // | Out-Null forces powershell to wait for msiexec to finish.
        auto preparePathForPowershell = [](QString path) {
            path.replace("'", "''");

            return QDir::toNativeSeparators(path);
        };

        QString msiLogFile = ConfigFile::configPath() + "msi.log";
        const QString command = QStringLiteral("&{msiexec /norestart /passive /i '%1' /L*V '%2'| Out-Null ; &'%3'}")
                                    .arg(preparePathForPowershell(updateFile),
                                        preparePathForPowershell(msiLogFile),
                                        preparePathForPowershell(QCoreApplication::applicationFilePath()));

        QProcess::startDetached("powershell.exe", QStringList{"-Command", command});
    }
}

void OCUpdater::checkForUpdate()
{
    QNetworkReply *reply = _accessManager->get(QNetworkRequest(_updateUrl));
    connect(_timeoutWatchdog, &QTimer::timeout, this, &OCUpdater::slotTimedOut);
    _timeoutWatchdog->start(30 * 1000);
    connect(reply, &QNetworkReply::finished, this, &OCUpdater::slotVersionInfoArrived);

    setDownloadState(CheckingServer);
}

void OCUpdater::slotOpenUpdateUrl()
{
    QDesktopServices::openUrl(_updateInfo.web());
}

bool OCUpdater::updateSucceeded() const
{
    QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);

    const auto targetVersionInt = QVersionNumber::fromString(settings.value(updateTargetVersionC).toString());
    return Version::versionWithBuildNumber() >= targetVersionInt;
}

void OCUpdater::slotVersionInfoArrived()
{
    _timeoutWatchdog->stop();
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcUpdater) << "Failed to reach version check url: " << reply->errorString();
        setDownloadState(OCUpdater::Unknown);
        return;
    }

    QString xml = QString::fromUtf8(reply->readAll());

    bool ok;
    _updateInfo = UpdateInfo::parseString(xml, &ok);
    if (ok) {
        versionInfoArrived(_updateInfo);
    } else {
        qCWarning(lcUpdater) << "Could not parse update information.";
        setDownloadState(OCUpdater::Unknown);
    }
}

void OCUpdater::slotTimedOut()
{
    setDownloadState(DownloadTimedOut);
}

////////////////////////////////////////////////////////////////////////

NSISUpdater::NSISUpdater(const QUrl &url)
    : OCUpdater(url)
{
}

void NSISUpdater::slotWriteFile()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (_file->isOpen()) {
        _file->write(reply->readAll());
    }
}

void NSISUpdater::wipeUpdateData()
{
    QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);
    QString updateFileName = settings.value(updateAvailableC).toString();
    if (!updateFileName.isEmpty())
        QFile::remove(updateFileName);
    settings.remove(updateAvailableC);
    settings.remove(updateTargetVersionC);
    settings.remove(updateTargetVersionStringC);
    settings.remove(autoUpdateAttemptedC);
}

void NSISUpdater::slotDownloadFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        setDownloadState(DownloadFailed);
        return;
    }

    QUrl url(reply->url());
    _file->close();

    QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);

    // remove previously downloaded but not used installer
    QFile oldTargetFile(settings.value(updateAvailableC).toString());
    if (oldTargetFile.exists()) {
        oldTargetFile.remove();
    }

    QFile::copy(_file->fileName(), _targetFile);
    setDownloadState(DownloadComplete);
    qCInfo(lcUpdater) << "Downloaded" << url.toString() << "to" << _targetFile;
    settings.setValue(updateTargetVersionC, updateInfo().version());
    settings.setValue(updateTargetVersionStringC, updateInfo().versionString());
    settings.setValue(updateAvailableC, _targetFile);
}

void NSISUpdater::versionInfoArrived(const UpdateInfo &info)
{
    QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);
    const auto infoVersion = QVersionNumber::fromString(info.version());
    const auto seenString = settings.value(seenVersionC).toString();
    const auto seenVersion = QVersionNumber::fromString(seenString);
    qCInfo(lcUpdater) << "Version info arrived:"
                      << "Your version:" << Version::versionWithBuildNumber()
                      << "Skipped version:" << seenVersion << seenString
                      << "Available version:" << infoVersion << info.version()
                      << "Available version string:" << info.versionString()
                      << "Web url:" << info.web()
                      << "Download url:" << info.downloadUrl();
    if (info.version().isEmpty())
    {
        qCInfo(lcUpdater) << "No version information available at the moment";
        setDownloadState(UpToDate);
    } else if (infoVersion <= Version::versionWithBuildNumber()
        || infoVersion <= seenVersion) {
        qCInfo(lcUpdater) << "Client is on latest version!";
        setDownloadState(UpToDate);
    } else {
        QString url = info.downloadUrl();
        if (url.isEmpty()) {
            showNoUrlDialog(info);
        } else {
            _targetFile = ConfigFile::configPath() + url.mid(url.lastIndexOf('/') + 1);
            if (QFile(_targetFile).exists()) {
                setDownloadState(DownloadComplete);
            } else {
                QNetworkReply *reply = qnam()->get(QNetworkRequest(QUrl(url)));
                connect(reply, &QIODevice::readyRead, this, &NSISUpdater::slotWriteFile);
                connect(reply, &QNetworkReply::finished, this, &NSISUpdater::slotDownloadFinished);
                setDownloadState(Downloading);
                _file.reset(new QTemporaryFile);
                _file->setAutoRemove(true);
                _file->open();
            }
        }
    }
}

void NSISUpdater::showNoUrlDialog(const UpdateInfo &info)
{
    // if the version tag is set, there is a newer version.
    QDialog *msgBox = new QDialog;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowFlags(msgBox->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QIcon infoIcon = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    int iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize);

    msgBox->setWindowIcon(infoIcon);

    QVBoxLayout *layout = new QVBoxLayout(msgBox);
    QHBoxLayout *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("New Version Available"));

    QLabel *ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(infoIcon.pixmap(iconSize));
    QLabel *lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available.</p>"
                     "<p><b>%2</b> is available for download. The installed version is %3.</p>")
                      .arg(Utility::escape(Theme::instance()->appNameGUI()),
                          Utility::escape(info.versionString()), Utility::escape(Version::versionWithBuildNumber().toString()));

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);

    QDialogButtonBox *bb = new QDialogButtonBox;
    QPushButton *skip = bb->addButton(tr("Skip this version"), QDialogButtonBox::ResetRole);
    QPushButton *reject = bb->addButton(tr("Skip this time"), QDialogButtonBox::AcceptRole);
    QPushButton *getupdate = bb->addButton(tr("Get update"), QDialogButtonBox::AcceptRole);

    connect(skip, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(reject, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(getupdate, &QAbstractButton::clicked, msgBox, &QDialog::accept);

    connect(skip, &QAbstractButton::clicked, this, &NSISUpdater::slotSetSeenVersion);
    connect(getupdate, &QAbstractButton::clicked, this, &NSISUpdater::slotOpenUpdateUrl);

    layout->addWidget(bb);

    msgBox->open();
    ownCloudGui::raiseDialog(msgBox);
}

void NSISUpdater::showUpdateErrorDialog(const QString &targetVersion)
{
    QDialog *msgBox = new QDialog;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowFlags(msgBox->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QIcon infoIcon = msgBox->style()->standardIcon(QStyle::SP_MessageBoxInformation);
    int iconSize = msgBox->style()->pixelMetric(QStyle::PM_MessageBoxIconSize);

    msgBox->setWindowIcon(infoIcon);

    QVBoxLayout *layout = new QVBoxLayout(msgBox);
    QHBoxLayout *hlayout = new QHBoxLayout;
    layout->addLayout(hlayout);

    msgBox->setWindowTitle(tr("Update Failed"));

    QLabel *ico = new QLabel;
    ico->setFixedSize(iconSize, iconSize);
    ico->setPixmap(infoIcon.pixmap(iconSize));
    QLabel *lbl = new QLabel;
    QString txt = tr("<p>A new version of the %1 Client is available but the updating process failed.</p>"
                     "<p><b>%2</b> has been downloaded. The installed version is %3.</p>")
                      .arg(Utility::escape(Theme::instance()->appNameGUI()),
                          Utility::escape(targetVersion), Utility::escape(Version::versionWithBuildNumber().toString()));

    lbl->setText(txt);
    lbl->setTextFormat(Qt::RichText);
    lbl->setWordWrap(true);

    hlayout->addWidget(ico);
    hlayout->addWidget(lbl);

    QDialogButtonBox *bb = new QDialogButtonBox;
    QPushButton *skip = bb->addButton(tr("Skip this version"), QDialogButtonBox::ResetRole);
    QPushButton *askagain = bb->addButton(tr("Ask again later"), QDialogButtonBox::ResetRole);
    QPushButton *retry = bb->addButton(tr("Restart and update"), QDialogButtonBox::AcceptRole);
    QPushButton *getupdate = bb->addButton(tr("Update manually"), QDialogButtonBox::AcceptRole);

    connect(skip, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(askagain, &QAbstractButton::clicked, msgBox, &QDialog::reject);
    connect(retry, &QAbstractButton::clicked, msgBox, &QDialog::accept);
    connect(getupdate, &QAbstractButton::clicked, msgBox, &QDialog::accept);

    connect(skip, &QAbstractButton::clicked, this, [this]() {
        wipeUpdateData();
        slotSetSeenVersion();
    });
    // askagain: do nothing
    connect(retry, &QAbstractButton::clicked, this, [this]() {
        slotStartInstaller();
        qApp->quit();
    });
    connect(getupdate, &QAbstractButton::clicked, this, [this]() {
        slotOpenUpdateUrl();
    });

    layout->addWidget(bb);

    msgBox->open();
}

bool NSISUpdater::handleStartup()
{
    ConfigFile cfg;
    QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);
    QString updateFileName = settings.value(updateAvailableC).toString();
    // has the previous run downloaded an update?
    if (!updateFileName.isEmpty() && QFile(updateFileName).exists()) {
        qCInfo(lcUpdater) << "An updater file is available";
        // did it try to execute the update?
        if (settings.value(autoUpdateAttemptedC, false).toBool()) {
            if (updateSucceeded()) {
                // success: clean up
                qCInfo(lcUpdater) << "The requested update attempt has succeeded"
                                  << Version::versionWithBuildNumber();
                wipeUpdateData();
                return false;
            } else {
                // auto update failed. Ask user what to do
                qCInfo(lcUpdater) << "The requested update attempt has failed"
                        << settings.value(updateTargetVersionC).toString();
                showUpdateErrorDialog(settings.value(updateTargetVersionStringC).toString());
                return false;
            }
        } else {
            qCInfo(lcUpdater) << "Triggering an update";
            return performUpdate();
        }
    }
    return false;
}

void NSISUpdater::slotSetSeenVersion()
{
    QSettings settings(ConfigFile::configFile(), QSettings::IniFormat);
    settings.setValue(seenVersionC, updateInfo().version());
}

////////////////////////////////////////////////////////////////////////

PassiveUpdateNotifier::PassiveUpdateNotifier(const QUrl &url)
    : OCUpdater(url)
{
    // remember the version of the currently running binary. On Linux it might happen that the
    // package management updates the package while the app is running. This is detected in the
    // updater slot: If the installed binary on the hd has a different version than the one
    // running, the running app is restarted. That happens in folderman.
    _runningAppVersion = Utility::versionOfInstalledBinary();
}

void PassiveUpdateNotifier::backgroundCheckForUpdate()
{
    if (Utility::isLinux()) {
        // on linux, check if the installed binary is still the same version
        // as the one that is running. If not, restart if possible.
        const QByteArray fsVersion = Utility::versionOfInstalledBinary();
        if (!(fsVersion.isEmpty() || _runningAppVersion.isEmpty()) && fsVersion != _runningAppVersion) {
            emit requestRestart();
        }
    }

    OCUpdater::backgroundCheckForUpdate();
}

void PassiveUpdateNotifier::versionInfoArrived(const UpdateInfo &info)
{
    const auto remoteVer = QVersionNumber::fromString(info.version());
    if (info.version().isEmpty() || Version::versionWithBuildNumber() >= remoteVer) {
        qCInfo(lcUpdater) << "Client is on latest version!";
        setDownloadState(UpToDate);
    } else {
        setDownloadState(UpdateOnlyAvailableThroughSystem);
    }
}

} // ns mirall
