/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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

#include "vfs_suffix.h"

#include <QFile>

#include "syncfileitem.h"
#include "filesystem.h"
#include "common/syncjournaldb.h"

namespace OCC {

VfsSuffix::VfsSuffix(QObject *parent)
    : Vfs(parent)
{
}

VfsSuffix::~VfsSuffix()
{
}

Vfs::Mode VfsSuffix::mode() const
{
    return WithSuffix;
}

QString VfsSuffix::fileSuffix() const
{
    return QStringLiteral(APPLICATION_DOTVIRTUALFILE_SUFFIX);
}

void VfsSuffix::startImpl(const VfsSetupParams &params)
{
    // It is unsafe for the database to contain any ".owncloud" file entries
    // that are not marked as a virtual file. These could be real .owncloud
    // files that were synced before vfs was enabled.
    QByteArrayList toWipe;
    params.journal->getFilesBelowPath("", [&toWipe](const SyncJournalFileRecord &rec) {
        if (!rec.isVirtualFile() && rec._path.endsWith(APPLICATION_DOTVIRTUALFILE_SUFFIX))
            toWipe.append(rec._path);
    });
    for (const auto &path : toWipe) {
        params.journal->deleteFileRecord(QString::fromUtf8(path));
    }
    Q_EMIT started();
}

void VfsSuffix::stop()
{
}

void VfsSuffix::unregisterFolder()
{
}

bool VfsSuffix::isHydrating() const
{
    return false;
}

Result<Vfs::ConvertToPlaceholderResult, QString> VfsSuffix::updateMetadata(const SyncFileItem &item, const QString &filePath, const QString &)
{
    FileSystem::setModTime(filePath, item._modtime);
    return Vfs::ConvertToPlaceholderResult::Ok;
}

Result<void, QString> VfsSuffix::createPlaceholder(const SyncFileItem &item)
{
    // The concrete shape of the placeholder is also used in isDehydratedPlaceholder() below
    const QString fn = _setupParams.filesystemPath + item._file;
    Q_ASSERT(fn.endsWith(fileSuffix()));

    QFile file(fn);
    if (file.exists() && file.size() > 1
        && !FileSystem::verifyFileUnchanged(fn, item._size, item._modtime)) {
        return QStringLiteral("Cannot create a placeholder because a file with the placeholder name already exist");
    }

    if (!file.open(QFile::ReadWrite | QFile::Truncate))
        return file.errorString();

    file.write(" ");
    file.close();
    FileSystem::setModTime(fn, item._modtime);
    return {};
}

Result<void, QString> VfsSuffix::dehydratePlaceholder(const SyncFileItem &item)
{
    SyncFileItem virtualItem(item);
    virtualItem._file = item._renameTarget;
    auto r = createPlaceholder(virtualItem);
    if (!r)
        return r;

    if (item._file != item._renameTarget) { // can be the same when renaming foo -> foo.owncloud to dehydrate
        QFile::remove(_setupParams.filesystemPath + item._file);
    }

    // Move the item's pin state
    auto pin = _setupParams.journal->internalPinStates().rawForPath(item._file.toUtf8());
    if (pin && *pin != PinState::Inherited) {
        setPinState(item._renameTarget, *pin);
        setPinState(item._file, PinState::Inherited);
    }

    // Ensure the pin state isn't contradictory
    pin = pinState(item._renameTarget);
    if (pin && *pin == PinState::AlwaysLocal)
        setPinState(item._renameTarget, PinState::Unspecified);
    return {};
}

bool VfsSuffix::isDehydratedPlaceholder(const QString &filePath)
{
    if (!filePath.endsWith(fileSuffix()))
        return false;
    QFileInfo fi(filePath);
    return fi.exists() && fi.size() == 1;
}

bool VfsSuffix::statTypeVirtualFile(csync_file_stat_t *stat, void *)
{
    if (stat->path.endsWith(fileSuffix().toUtf8())) {
        stat->type = ItemTypeVirtualFile;
        return true;
    }
    return false;
}

Vfs::AvailabilityResult VfsSuffix::availability(const QString &folderPath)
{
    return availabilityInDb(folderPath);
}

QString VfsSuffix::underlyingFileName(const QString &fileName) const
{
    {
        if (fileName.endsWith(fileSuffix())) {
            return fileName.chopped(fileSuffix().size());
        }
        return fileName;
    }
}

} // namespace OCC
