#include "jsontheme.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaEnum>

#include "common/asserts.h"
#include "config.h"

OCC::JsonTheme::JsonTheme()
{
    QFile file(brandThemePath() + QStringLiteral("/config_with_defaults.json"));
    qDebug() << file.fileName();
    OC_ENFORCE_X(file.open(QFile::ReadOnly), qUtf8Printable(file.errorString()));
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    OC_ENFORCE_X(error.error == QJsonParseError::NoError, qUtf8Printable(error.errorString()));
    _data = doc.object();

    // cache some
    _appNameGUI = _data.value(QLatin1String("desktop_application_name_text")).toString();
    _appName = _data.value(QLatin1String("desktop_application_short_name_text")).toString();
}

QString OCC::JsonTheme::defaultServerFolder() const
{
    return _data.value(QLatin1String("desktop_sync_folder_target_text")).toString();
}

QString OCC::JsonTheme::quotaBaseFolder() const
{
    return _data.value(QLatin1String("desktop_quota_base_folder_text")).toString();
}

QString OCC::JsonTheme::overrideServerUrl() const
{
    return _data.value(QLatin1String("desktop_server_url_text")).toString();
}

QString OCC::JsonTheme::helpUrl() const
{
    return _data.value(QLatin1String("desktop_settings_help_url_text")).toString();
}

QString OCC::JsonTheme::conflictHelpUrl() const
{
    return QString();
}

QString OCC::JsonTheme::wizardUrlPostfix() const
{
    return _data.value(QLatin1String("desktop_partial_server_url_text")).toString();
}

QColor OCC::JsonTheme::wizardHeaderBackgroundColor() const
{
    return QColor(_data.value(QLatin1String("desktop_backlog_color")).toString());
}

QColor OCC::JsonTheme::wizardHeaderTitleColor() const
{
    return QColor(_data.value(QLatin1String("desktop_title_color")).toString());
}

QString OCC::JsonTheme::wizardUrlHint() const
{
    return _data.value(QLatin1String("desktop_server_url_hint_text")).toString();
}

bool OCC::JsonTheme::forceSystemNetworkProxy() const
{
    return _data.value(QLatin1String("desktop_force_system_proxy_check")).toBool();
}

bool OCC::JsonTheme::singleSyncFolder() const
{
    return _data.value(QLatin1String("desktop_sync_just_on_folder_check")).toBool();
}

bool OCC::JsonTheme::multiAccount() const
{
    return _data.value(QLatin1String("desktop_settings_multiaccount_check")).toBool();
}

bool OCC::JsonTheme::userGroupSharing() const
{
    return _data.value(QLatin1String("desktop_user_group_sharing_check")).toBool();
}

bool OCC::JsonTheme::linkSharing() const
{
    return _data.value(QLatin1String("desktop_link_sharing_check")).toBool();
}

OCC::Theme::UserIDType OCC::JsonTheme::userIDType() const
{
    return Utility::stringToEnum<OCC::Theme::UserIDType>(_data.value(QLatin1String("desktop_user_id_type_select")).toString());
}

QString OCC::JsonTheme::customUserID() const
{
    return _data.value(QLatin1String("desktop_custom_user_id_text")).toString();
}

QString OCC::JsonTheme::appNameGUI() const
{
    return _appNameGUI;
}

QString OCC::JsonTheme::userIDHint() const
{
    return _data.value(QLatin1String("desktop_user_id_hint_text")).toString();
}

QIcon OCC::JsonTheme::wizardHeaderLogo() const
{
    return themeUniversalIcon(_data.value(QLatin1String("desktop_wizard_logo_image")).toString());
}

QColor OCC::JsonTheme::wizardHeaderSubTitleColor() const
{
    // TODO: was not provided by ownbrander
    const auto val = _data.constFind(QLatin1String("desktop_backlog_color_subtitle"));
    if (val != _data.constEnd()) {
        return QColor(val->toString());
    }
    return wizardHeaderTitleColor();
}

QString OCC::JsonTheme::updateCheckUrl() const
{
    return _data.value(QLatin1String("desktop_update_url_text")).toString();
}

QString OCC::JsonTheme::appName() const
{
    return _appName;
}

QString OCC::JsonTheme::webDavPath() const
{
    return _data.value(QLatin1String("desktop_webdav_url_text")).toString();
}

qint64 OCC::JsonTheme::newBigFolderSizeLimit() const
{
    // TODO: does that work?
    return _data.value(QLatin1String("desktop_sync_folder_limit_text")).toVariant().value<quint64>();
}

bool OCC::JsonTheme::wizardHideExternalStorageConfirmationCheckbox() const
{
    return _data.value(QLatin1String("desktop_hide_external_storage_setting_check")).toBool();
}

bool OCC::JsonTheme::wizardHideFolderSizeLimitCheckbox() const
{
    return _data.value(QLatin1String("desktop_hide_folder_size_limit_setting_check")).toBool();
}

QString OCC::JsonTheme::oauthClientId() const
{
    return _data.value(QLatin1String("desktop_settings_oauth2_clientid_text")).toString();
}

QString OCC::JsonTheme::oauthClientSecret() const
{
    return _data.value(QLatin1String("desktop_settings_oauth2_secretid_text")).toString();
}

QString OCC::JsonTheme::openIdConnectScopes() const
{
    return _data.value(QLatin1String("desktop_settings_oidc_scopes_text")).toString();
}

QString OCC::JsonTheme::about() const
{
    // TODO: Get rid of code?
    QString devString = tr("<p>Version %1. For more information visit <a href=\"https://%2\">https://%2</a></p>").arg(Utility::escape(QStringLiteral(MIRALL_VERSION_STRING)), Utility::escape(QStringLiteral(APPLICATION_DOMAIN)));

    if (!_data.value(QLatin1String("desktop_about_text_check")).toBool()) {
        devString += tr("<p>Copyright ownCloud GmbH</p>");
        devString += tr("<p>Distributed by %1 and licensed under the GNU General Public License (GPL) Version 2.0.<br/>")
                         .arg(Utility::escape(QStringLiteral(APPLICATION_VENDOR)));
        devString += gitSHA1();
    }
    return devString;
}

bool OCC::JsonTheme::aboutShowCopyright() const
{
    return _data.value(QLatin1String("desktop_about_text_check")).toBool();
}

bool OCC::JsonTheme::showVirtualFilesOption() const
{
    return _data.value(QLatin1String("desktop_vfs_enable_check")).toBool();
}

bool OCC::JsonTheme::forceVirtualFilesOption() const
{
    return _data.value(QLatin1String("desktop_vfs_enforce_check")).toBool();
}

bool OCC::JsonTheme::enableExperimentalFeatures() const
{
    // TODO: Maybe replace by indivitual flags?
    return _data.value(QLatin1String("desktop_experimental_check")).toBool();
}
