/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#include "sharedialog.h"
#include "sharee.h"
#include "sharelinkwidget.h"
#include "shareusergroupwidget.h"
#include "ui_sharedialog.h"

#include "account.h"
#include "accountstate.h"
#include "application.h"
#include "configfile.h"
#include "settingsdialog.h"
#include "theme.h"
#include "thumbnailjob.h"

#include <QFileInfo>
#include <QFileIconProvider>
#include <QPointer>
#include <QPushButton>
#include <QFrame>
#include <QRegularExpression>

namespace OCC {

static const int thumbnailSize = 40;

ShareDialog::ShareDialog(QPointer<AccountState> accountState,
    const QString &sharePath,
    const QString &localPath,
    SharePermissions maxSharingPermissions,
    ShareDialogStartPage startPage,
    QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::ShareDialog)
    , _accountState(accountState)
    , _sharePath(sharePath)
    , _localPath(localPath)
    , _maxSharingPermissions(maxSharingPermissions)
    , _startPage(startPage)
    , _linkWidget(nullptr)
    , _userGroupWidget(nullptr)
    , _progressIndicator(nullptr)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("SharingDialog");

    _ui->setupUi(this);

    QPushButton *closeButton = _ui->buttonBox->button(QDialogButtonBox::Close);
    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);

    // We want to act on account state changes
    connect(_accountState.data(), &AccountState::stateChanged, this, &ShareDialog::slotAccountStateChanged);

    // Because people press enter in the dialog and we don't want to close for that
    closeButton->setDefault(false);
    closeButton->setAutoDefault(false);

    // Set icon
    QFileInfo f_info(_localPath);
    QFileIconProvider icon_provider;
    const QIcon icon = icon_provider.icon(f_info);
    if (!icon.isNull()) {
        auto pixmap = icon.pixmap(thumbnailSize, thumbnailSize);
        _ui->label_icon->setPixmap(pixmap);
    } else {
        _ui->label_icon->hide();
    }

    // Set filename
    QString fileName = QFileInfo(_sharePath).fileName();
    _ui->label_name->setText(tr("%1").arg(fileName));

    _ui->label_sharePath->setWordWrap(true);
    QString ocDir(_sharePath);
    ocDir.truncate(ocDir.length() - fileName.length());

    // remove leading and trailing spaces
    ocDir.remove(QRegularExpression(QStringLiteral("^/*|/*$")));

    if (ocDir.isEmpty()) {
        _ui->label_name->setVisible(true);
        _ui->label_sharePath->setVisible(false);
        _ui->label_sharePath->setText(QString());
    } else {
        _ui->label_name->setVisible(true);
        _ui->label_sharePath->setVisible(true);
        _ui->label_sharePath->setText(tr("Folder: %2").arg(ocDir));
    }

    this->setWindowTitle(tr("%1 Sharing").arg(Theme::instance()->appNameGUI()));

    if (!accountState->account()->capabilities().shareAPI()) {
        auto label = new QLabel(tr("The server does not allow sharing"));
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout()->replaceWidget(_ui->shareWidgets, label);
        _ui->shareWidgets->hide();
        return;
    }

    if (QFileInfo(_localPath).isFile()) {
        ThumbnailJob *job = new ThumbnailJob(_sharePath, _accountState->account(), this);
        connect(job, &ThumbnailJob::jobFinished, this, &ShareDialog::slotThumbnailFetched);
        job->start();
    }

    _progressIndicator = new QProgressIndicator(this);
    _progressIndicator->startAnimation();
    _progressIndicator->setToolTip(tr("Retrieving maximum possible sharing permissions from server..."));
    _ui->buttonBoxLayout->insertWidget(0, _progressIndicator);

    // Server versions >= 9.1 support the "share-permissions" property
    // older versions will just return share-permissions: ""
    auto job = new PropfindJob(accountState->account(), _sharePath);
    job->setProperties({ QByteArrayLiteral("http://open-collaboration-services.org/ns:share-permissions"),
        QByteArrayLiteral("http://owncloud.org/ns:privatelink") });
    job->setTimeout(10 * 1000);
    connect(job, &PropfindJob::result, this, &ShareDialog::slotPropfindReceived);
    connect(job, &PropfindJob::finishedWithError, this, &ShareDialog::slotPropfindError);
    job->start();

    resize(ocApp()->gui()->settingsDialog()->sizeHintForChild());
}

ShareDialog::~ShareDialog()
{
    delete _ui;
}

void ShareDialog::slotPropfindReceived(const QMap<QString, QString> &result)
{
    const auto &receivedPermissions = result["share-permissions"];
    if (!receivedPermissions.isEmpty()) {
        _maxSharingPermissions = static_cast<SharePermissions>(receivedPermissions.toInt());
        qCInfo(lcSharing) << "Received sharing permissions for" << _sharePath << _maxSharingPermissions;
    }
    auto privateLinkUrl = result["privatelink"];
    if (!privateLinkUrl.isEmpty()) {
        qCInfo(lcSharing) << "Received private link url for" << _sharePath << privateLinkUrl;
        _privateLinkUrl = privateLinkUrl;
    }

    showSharingUi();
}

void ShareDialog::slotPropfindError()
{
    // On error show the share ui anyway. The user can still see shares,
    // delete them and so on, even though adding new shares or granting
    // some of the permissions might fail.

    showSharingUi();
}

void ShareDialog::showSharingUi()
{
    _progressIndicator->stopAnimation();

    auto theme = Theme::instance();

    // There's no difference between being unable to reshare and
    // being unable to reshare with reshare permission.
    bool canReshare = _maxSharingPermissions & SharePermissionShare;

    if (!canReshare) {
        auto label = new QLabel(this);
        label->setText(tr("The file can not be shared because it was shared without sharing permission."));
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        layout()->replaceWidget(_ui->shareWidgets, label);
        return;
    }

    // We only do user/group sharing from 8.2.0
    bool userGroupSharing =
        theme->userGroupSharing()
        && _accountState->account()->serverVersionInt() >= Account::makeServerVersion(8, 2, 0);

    if (userGroupSharing) {
        _userGroupWidget = new ShareUserGroupWidget(_accountState->account(), _sharePath, _localPath, _maxSharingPermissions, _privateLinkUrl, this);
        _ui->shareWidgets->addTab(_userGroupWidget, tr("Users and Groups"));
        _userGroupWidget->getShares();
    }

    if (theme->linkSharing()) {
        _linkWidget = new ShareLinkWidget(_accountState->account(), _sharePath, _localPath, _maxSharingPermissions, this);
        _linkWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        _ui->shareWidgets->addTab(_linkWidget, tr("Public Links"));
        _linkWidget->getShares();

        if (_startPage == ShareDialogStartPage::PublicLinks)
            _ui->shareWidgets->setCurrentWidget(_linkWidget);
    }
}

QSize ShareDialog::minimumSizeHint() const
{
    return ocApp()->gui()->settingsDialog()->sizeHintForChild();
}

void ShareDialog::slotThumbnailFetched(const int &statusCode, const QPixmap &reply)
{
    if (statusCode != 200) {
        qCWarning(lcSharing) << "Thumbnail status code: " << statusCode;
        return;
    }
    if (reply.isNull()) {
        qCWarning(lcSharing) << "Invalid pixmap";
        return;
    }
    const auto p = reply.scaledToHeight(thumbnailSize, Qt::SmoothTransformation);
    _ui->label_icon->setPixmap(p);
    _ui->label_icon->show();
}

void ShareDialog::slotAccountStateChanged(int state)
{
    bool enabled = (state == AccountState::State::Connected);
    qCDebug(lcSharing) << "Account connected?" << enabled;

    if (_userGroupWidget != nullptr) {
        _userGroupWidget->setEnabled(enabled);
    }

    if (_linkWidget != nullptr) {
        _linkWidget->setEnabled(enabled);
    }
}
}
