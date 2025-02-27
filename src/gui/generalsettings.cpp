/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "generalsettings.h"
#include "ui_generalsettings.h"

#include "accountmanager.h"
#include "application.h"
#include "common/version.h"
#include "configfile.h"
#include "owncloudsetupwizard.h"
#include "theme.h"

#include "updater/updater.h"
#include "updater/ocupdater.h"
#ifdef Q_OS_MAC
// FIXME We should unify those, but Sparkle does everything behind the scene transparently
#include "updater/sparkleupdater.h"
#endif
#include "ignorelisteditor.h"

#include "config.h"
#include "translations.h"

#include <QNetworkProxy>
#include <QDir>
#include <QScopedValueRollback>
#include <QMessageBox>
#include <QOperatingSystemVersion>

namespace OCC {

GeneralSettings::GeneralSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::GeneralSettings)
    , _currentlyLoading(false)
{
    _ui->setupUi(this);

    connect(_ui->desktopNotificationsCheckBox, &QAbstractButton::toggled,
        this, &GeneralSettings::slotToggleOptionalDesktopNotifications);
#ifdef Q_OS_WIN
    connect(_ui->showInExplorerNavigationPaneCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotShowInExplorerNavigationPane);
#endif

    reloadConfig();
    loadMiscSettings();
    slotUpdateInfo();

    // misc
    connect(_ui->monoIconsCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->crashreporterCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newFolderLimitCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newFolderLimitSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &GeneralSettings::saveMiscSettings);
    connect(_ui->newExternalStorage, &QAbstractButton::toggled, this, &GeneralSettings::saveMiscSettings);

    connect(_ui->languageDropdown, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        // first, store selected language in config file
        saveMiscSettings();

        // warn user that a language change requires a restart to take effect
        QMessageBox::warning(this, tr("Warning"), tr("Language changes require a restart of this application to take effect."), QMessageBox::Ok);
    });

    /* handle the hidden file checkbox */

    /* the ignoreHiddenFiles flag is a folder specific setting, but for now, it is
     * handled globally. Save it to every folder that is defined.
     */
    connect(_ui->syncHiddenFilesCheckBox, &QCheckBox::toggled, this,[](bool checked){
        FolderMan::instance()->setIgnoreHiddenFiles(!checked);
    });

#ifndef WITH_CRASHREPORTER
    _ui->crashreporterCheckBox->setVisible(false);
#endif

    // Hide on non-Windows, or WindowsVersion < 10.
    // The condition should match the default value of ConfigFile::showInExplorerNavigationPane.
    _ui->showInExplorerNavigationPaneCheckBox->setVisible(QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows10);

    /* Set the left contents margin of the layout to zero to make the checkboxes
     * align properly vertically , fixes bug #3758
     */
    int m0, m1, m2, m3;
    _ui->horizontalLayout_3->getContentsMargins(&m0, &m1, &m2, &m3);
    _ui->horizontalLayout_3->setContentsMargins(0, m1, m2, m3);

    // OEM themes are not obliged to ship mono icons, so there
    // is no point in offering an option
    _ui->monoIconsCheckBox->setVisible(Theme::instance()->monoIconsAvailable());

    connect(_ui->ignoredFilesButton, &QAbstractButton::clicked, this, &GeneralSettings::slotIgnoreFilesEditor);
    connect(_ui->logSettingsButton, &QPushButton::clicked, this, [] {
        // only access occApp after things are set up
        ocApp()->gui()->slotToggleLogBrowser();
    });

    // accountAdded means the wizard was finished and the wizard might change some options.
    connect(AccountManager::instance(), &AccountManager::accountAdded, this, &GeneralSettings::loadMiscSettings);

    // Only our standard brandings currently support beta channel
    if (Theme::instance()->appName() != QLatin1String("testpilotcloud")) {
#ifdef Q_OS_MAC
        // Because we don't have any statusString from the SparkleUpdater anyway we can hide the whole thing
        _ui->updaterWidget->hide();
#else
        _ui->updateChannelLabel->hide();
        _ui->updateChannel->hide();
        if (ConfigFile().updateChannel() != QLatin1String("stable")) {
            ConfigFile().setUpdateChannel(QStringLiteral("stable"));
        }
#endif
    }
    connect(_ui->about_pushButton, &QPushButton::clicked, this, &GeneralSettings::showAbout);

    if (!Theme::instance()->aboutShowCopyright()) {
        _ui->copyrightLabel->hide();
    }
    if (Theme::instance()->forceVirtualFilesOption()) {
        _ui->groupBox_non_vfs->hide();
    }
}

GeneralSettings::~GeneralSettings()
{
    delete _ui;
}

void GeneralSettings::loadMiscSettings()
{
    QScopedValueRollback<bool> scope(_currentlyLoading, true);
    ConfigFile cfgFile;
    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
    _ui->desktopNotificationsCheckBox->setChecked(cfgFile.optionalDesktopNotifications());
    _ui->showInExplorerNavigationPaneCheckBox->setChecked(cfgFile.showInExplorerNavigationPane());
    _ui->crashreporterCheckBox->setChecked(cfgFile.crashReporter());
    auto newFolderLimit = cfgFile.newBigFolderSizeLimit();
    _ui->newFolderLimitCheckBox->setChecked(newFolderLimit.first);
    _ui->newFolderLimitSpinBox->setValue(newFolderLimit.second);
    _ui->newExternalStorage->setChecked(cfgFile.confirmExternalStorage());
    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());

    // the dropdown has to be populated before we can can pick an entry below based on the stored setting
    loadLanguageNamesIntoDropdown();

    const auto &locale = cfgFile.uiLanguage();

    // index 0 means "use default", which we use unless the loop below sets another entry
    _ui->languageDropdown->setCurrentIndex(0);

    if (!locale.isEmpty()) {
        const auto &language = QLocale(locale).nativeLanguageName();

        // a simple linear search to find the right entry and choose it is sufficient for this application
        // we can skip the "use default" entry by starting at index 1
        // note that if the loop below never breaks, the setting falls back to "use default"
        // this is desired behavior, as it handles cases when the selected language no longer exists
        for (int i = 1; i < _ui->languageDropdown->count(); ++i) {
            const auto text = _ui->languageDropdown->itemText(i);

            if (text == language) {
                _ui->languageDropdown->setCurrentIndex(i);
                break;
            }
        }
    }
}

void GeneralSettings::showEvent(QShowEvent *)
{
    reloadConfig();
}

void GeneralSettings::slotUpdateInfo()
{
    if (ConfigFile().skipUpdateCheck() || !Updater::instance()) {
        // updater disabled on compile
        _ui->updaterWidget->setVisible(false);
        return;
    }

    // Note: the sparkle-updater is not an OCUpdater
    OCUpdater *ocupdater = qobject_cast<OCUpdater *>(Updater::instance());
    if (ocupdater) {
        connect(ocupdater, &OCUpdater::downloadStateChanged, this, &GeneralSettings::slotUpdateInfo, Qt::UniqueConnection);
        connect(_ui->restartButton, &QAbstractButton::clicked, ocupdater, &OCUpdater::slotStartInstaller, Qt::UniqueConnection);
        connect(_ui->restartButton, &QAbstractButton::clicked, qApp, &QApplication::quit, Qt::UniqueConnection);

        _ui->updateStateLabel->setText(ocupdater->statusString());
        _ui->restartButton->setVisible(ocupdater->downloadState() == OCUpdater::DownloadComplete);

    }
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
    else if (SparkleUpdater *sparkleUpdater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
        _ui->updateStateLabel->setText(sparkleUpdater->statusString());
        _ui->restartButton->setVisible(false);
    }
#endif

    // Channel selection
    _ui->updateChannel->setCurrentIndex(ConfigFile().updateChannel() == "beta" ? 1 : 0);
    connect(_ui->updateChannel, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
        this, &GeneralSettings::slotUpdateChannelChanged, Qt::UniqueConnection);
}

void GeneralSettings::slotUpdateChannelChanged(int index)
{
    QString channel = index == 0 ? QStringLiteral("stable") : QStringLiteral("beta");
    if (channel == ConfigFile().updateChannel())
        return;

    auto msgBox = new QMessageBox(
        QMessageBox::Warning,
        tr("Change update channel?"),
        tr("The update channel determines which client updates will be offered "
           "for installation. The \"stable\" channel contains only upgrades that "
           "are considered reliable, while the versions in the \"beta\" channel "
           "may contain newer features and bugfixes, but have not yet been tested "
           "thoroughly."
           "\n\n"
           "Note that this selects only what pool upgrades are taken from, and that "
           "there are no downgrades: So going back from the beta channel to "
           "the stable channel usually cannot be done immediately and means waiting "
           "for a stable version that is newer than the currently installed beta "
           "version."),
        QMessageBox::NoButton,
        this);
    auto acceptButton = msgBox->addButton(tr("Change update channel"), QMessageBox::AcceptRole);
    msgBox->addButton(tr("Cancel"), QMessageBox::RejectRole);
    connect(msgBox, &QMessageBox::finished, msgBox, [this, channel, msgBox, acceptButton] {
        msgBox->deleteLater();
        if (msgBox->clickedButton() == acceptButton) {
            ConfigFile().setUpdateChannel(channel);
            if (OCUpdater *updater = qobject_cast<OCUpdater *>(Updater::instance())) {
                updater->setUpdateUrl(Updater::updateUrl());
                updater->checkForUpdate();
            }
#if defined(Q_OS_MAC) && defined(HAVE_SPARKLE)
            else if (SparkleUpdater *updater = qobject_cast<SparkleUpdater *>(Updater::instance())) {
                updater->setUpdateUrl(Updater::updateUrl());
                updater->checkForUpdate();
            }
#endif
        } else {
            _ui->updateChannel->setCurrentText(ConfigFile().updateChannel());
        }
    });
    msgBox->open();
}

void GeneralSettings::saveMiscSettings()
{
    if (_currentlyLoading)
        return;
    ConfigFile cfgFile;
    bool isChecked = _ui->monoIconsCheckBox->isChecked();
    cfgFile.setMonoIcons(isChecked);
    Theme::instance()->setSystrayUseMonoIcons(isChecked);
    cfgFile.setCrashReporter(_ui->crashreporterCheckBox->isChecked());

    cfgFile.setNewBigFolderSizeLimit(_ui->newFolderLimitCheckBox->isChecked(),
        _ui->newFolderLimitSpinBox->value());
    cfgFile.setConfirmExternalStorage(_ui->newExternalStorage->isChecked());

    const auto pickedLanguageIndex = _ui->languageDropdown->currentIndex();

    // the first entry, identified by index 0, means "use default", which is a special case handled below
    if (pickedLanguageIndex > 0) {
        // for now, we use the locale names as labels in the dropdown
        // therefore, we can store them directly in the config file
        // in future versions, we will likely display nice names instead of locales to improve the user experience
        const auto pickedLanguageName = _ui->languageDropdown->itemText(pickedLanguageIndex);
        const auto pickedLanguageLocale = localesToLanguageNamesMap.key(pickedLanguageName);

        cfgFile.setUiLanguage(pickedLanguageLocale);
    } else {
        // empty string means "use system default"
        cfgFile.setUiLanguage("");
    }
}

void GeneralSettings::slotToggleLaunchOnStartup(bool enable)
{
    Theme *theme = Theme::instance();
    Utility::setLaunchOnStartup(theme->appName(), theme->appNameGUI(), enable);
}

void GeneralSettings::slotToggleOptionalDesktopNotifications(bool enable)
{
    ConfigFile cfgFile;
    cfgFile.setOptionalDesktopNotifications(enable);
}

#ifdef Q_OS_WIN
void GeneralSettings::slotShowInExplorerNavigationPane(bool checked)
{
    ConfigFile cfgFile;
    cfgFile.setShowInExplorerNavigationPane(checked);
    // Now update the registry with the change.
    FolderMan::instance()->navigationPaneHelper().setShowInExplorerNavigationPane(checked);
}
#endif

void GeneralSettings::slotIgnoreFilesEditor()
{
    if (_ignoreEditor.isNull()) {
        _ignoreEditor = new IgnoreListEditor(this);
        _ignoreEditor->setAttribute(Qt::WA_DeleteOnClose, true);
        _ignoreEditor->open();
    } else {
        ownCloudGui::raiseDialog(_ignoreEditor);
    }
}

void GeneralSettings::reloadConfig()
{
    _ui->syncHiddenFilesCheckBox->setChecked(!FolderMan::instance()->ignoreHiddenFiles());
    if (Utility::hasSystemLaunchOnStartup(Theme::instance()->appName())) {
        _ui->autostartCheckBox->setChecked(true);
        _ui->autostartCheckBox->setDisabled(true);
        _ui->autostartCheckBox->setToolTip(tr("You cannot disable autostart because system-wide autostart is enabled."));
    } else {
        const bool hasAutoStart = Utility::hasLaunchOnStartup(Theme::instance()->appName());
        // make sure the binary location is correctly set
        slotToggleLaunchOnStartup(hasAutoStart);
        _ui->autostartCheckBox->setChecked(hasAutoStart);
        connect(_ui->autostartCheckBox, &QAbstractButton::toggled, this, &GeneralSettings::slotToggleLaunchOnStartup);
    }
}

void GeneralSettings::loadLanguageNamesIntoDropdown()
{
    // initialize map of locales to language names
    const auto availableLocales = Translations::listAvailableTranslations();
    for (const auto &availableLocale : availableLocales) {
        auto nativeLanguageName = QLocale(availableLocale).nativeLanguageName();

        // fallback if there's a locale whose name Qt doesn't know
        // this indicates a broken filename
        if (nativeLanguageName.isEmpty()) {
            qCDebug(lcApplication()) << "Warning: could not find native language name for locale" << availableLocale;
            nativeLanguageName = tr("unknown (%1)").arg(availableLocale);
        }

        localesToLanguageNamesMap.insert(availableLocale, nativeLanguageName);
    }

    // allow method to be called more than once
    _ui->languageDropdown->clear();

    // if no option has been chosen explicitly by the user, the first entry shall be used
    _ui->languageDropdown->addItem(tr("(use default)"));

    QStringList availableTranslations(localesToLanguageNamesMap.values());
    availableTranslations.sort(Qt::CaseInsensitive);

    for (const auto &i : qAsConst(availableTranslations)) {
        _ui->languageDropdown->addItem(i);
    }
}

} // namespace OCC
