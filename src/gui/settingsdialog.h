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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QMainWindow>
#include <QStyledItemDelegate>

#include "progressdispatcher.h"
#include "owncloudgui.h"

class QAction;
class QActionGroup;
class QToolBar;
class QStandardItemModel;

namespace OCC {

class AccountState;

namespace Ui {
    class SettingsDialog;
}
class AccountSettings;
class Application;
class FolderMan;
class ownCloudGui;
class ActivitySettings;

/**
 * @brief The SettingsDialog class
 * @ingroup gui
 */
class SettingsDialog : public QMainWindow
{
    Q_OBJECT
    Q_PROPERTY(QWidget* currentPage READ currentPage)

public:
    explicit SettingsDialog(ownCloudGui *gui, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    QSize sizeHintForChild() const;

    QWidget* currentPage();

public slots:
    void showFirstPage();
    void showActivityPage();
    void showIssuesList();
    void slotSwitchPage(QAction *action);
    void slotRefreshActivity(AccountState *accountState);
    void slotRefreshActivityAccountStateSender();
    void slotAccountAvatarChanged();
    void slotAccountDisplayNameChanged();

protected:
    void changeEvent(QEvent *) override;
    void setVisible(bool visible) override;

#if defined(Q_OS_WIN) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private slots:
    void accountAdded(AccountState *);
    void accountRemoved(AccountState *);

private:
    void customizeStyle();

    QAction *createActionWithIcon(const QString &iconName, const QString &text);

    Ui::SettingsDialog *const _ui;

    QActionGroup *_actionGroup;
    // Maps the actions from the action group to the corresponding widgets
    QHash<QAction *, QWidget *> _actionGroupWidgets;

    // Maps the action in the dialog to their according account. Needed in
    // case the account avatar changes
    QHash<Account *, QAction *> _actionForAccount;

    ActivitySettings *_activitySettings;

    QAction *_activityAction;
    QAction *_addAccountAction = nullptr;
    ownCloudGui *_gui;

};
}

#endif // SETTINGSDIALOG_H
