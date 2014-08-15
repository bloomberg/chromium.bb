// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/deferred_on_disk_directory_backing_store.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "sync/syncable/syncable-inl.h"

namespace syncer {
namespace syncable {

DeferredOnDiskDirectoryBackingStore::DeferredOnDiskDirectoryBackingStore(
    const std::string& dir_name, const base::FilePath& backing_filepath)
    : DirectoryBackingStore(dir_name),
      backing_filepath_(backing_filepath),
      db_is_on_disk_(false) {
}

DeferredOnDiskDirectoryBackingStore::~DeferredOnDiskDirectoryBackingStore() {}

bool DeferredOnDiskDirectoryBackingStore::SaveChanges(
    const Directory::SaveChangesSnapshot& snapshot) {
  DCHECK(CalledOnValidThread());

  // Back out early if there is nothing to save.
  if (snapshot.dirty_metas.empty() && snapshot.metahandles_to_purge.empty() &&
      snapshot.delete_journals.empty() &&
      snapshot.delete_journals_to_purge.empty()) {
    return true;
  }

  if (!db_is_on_disk_) {
    if (!base::DeleteFile(backing_filepath_, false))
      return false;

    // Reopen DB on disk.
    db_.reset(new sql::Connection);
    db_->set_exclusive_locking();
    db_->set_page_size(4096);
    if (!db_->Open(backing_filepath_) || !InitializeTables())
      return false;

    db_is_on_disk_ = true;
  }

  return DirectoryBackingStore::SaveChanges(snapshot);
}

DirOpenResult DeferredOnDiskDirectoryBackingStore::Load(
    Directory::MetahandlesMap* handles_map,
    JournalIndex* delete_journals,
    Directory::KernelLoadInfo* kernel_load_info) {
  // Open an in-memory database at first to create initial sync data needed by
  // Directory.
  CHECK(!db_->is_open());
  if (!db_->OpenInMemory())
    return FAILED_OPEN_DATABASE;

  if (!InitializeTables())
    return FAILED_OPEN_DATABASE;
  if (!LoadEntries(handles_map))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadInfo(kernel_load_info))
    return FAILED_DATABASE_CORRUPT;

  return OPENED;
}

}  // namespace syncable
}  // namespace syncer
