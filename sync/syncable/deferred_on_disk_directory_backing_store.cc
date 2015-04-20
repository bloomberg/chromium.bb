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
    const std::string& dir_name,
    const base::FilePath& backing_file_path)
    : OnDiskDirectoryBackingStore(dir_name, backing_file_path),
      created_on_disk_(false) {
}

DeferredOnDiskDirectoryBackingStore::~DeferredOnDiskDirectoryBackingStore() {}

bool DeferredOnDiskDirectoryBackingStore::SaveChanges(
    const Directory::SaveChangesSnapshot& snapshot) {
  DCHECK(CalledOnValidThread());

  // Back out early if there is nothing to save.
  if (!snapshot.HasUnsavedMetahandleChanges()) {
    return true;
  }
  if (!created_on_disk_ && !CreateOnDisk())
    return false;
  return OnDiskDirectoryBackingStore::SaveChanges(snapshot);
}

bool DeferredOnDiskDirectoryBackingStore::CreateOnDisk() {
  DCHECK(CalledOnValidThread());
  DCHECK(!created_on_disk_);
  ResetAndCreateConnection();
  if (!base::DeleteFile(backing_file_path(), false))
    return false;
  if (!Open(backing_file_path()) || !InitializeTables())
    return false;
  created_on_disk_ = true;
  return true;
}

DirOpenResult DeferredOnDiskDirectoryBackingStore::Load(
    Directory::MetahandlesMap* handles_map,
    JournalIndex* delete_journals,
    MetahandleSet* metahandles_to_purge,
    Directory::KernelLoadInfo* kernel_load_info) {
  DCHECK(CalledOnValidThread());
  // Open an in-memory database at first to create initial sync data needed by
  // Directory.
  CHECK(!IsOpen());
  if (!OpenInMemory())
    return FAILED_OPEN_DATABASE;

  if (!InitializeTables())
    return FAILED_OPEN_DATABASE;
  if (!LoadEntries(handles_map, metahandles_to_purge))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadInfo(kernel_load_info))
    return FAILED_DATABASE_CORRUPT;

  return OPENED;
}

}  // namespace syncable
}  // namespace syncer
