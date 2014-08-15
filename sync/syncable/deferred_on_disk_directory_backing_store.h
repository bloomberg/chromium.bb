// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_DEFERRED_ON_DISK_DIRECTORY_BACKING_STORE_H_
#define SYNC_SYNCABLE_DEFERRED_ON_DISK_DIRECTORY_BACKING_STORE_H_

#include "base/files/file_path.h"
#include "sync/base/sync_export.h"
#include "sync/syncable/directory_backing_store.h"

namespace syncer {
namespace syncable {

// Store used for backing up user's data before first sync. It creates an
// in-memory store first and switch to on-disk store if SaveChanges() is
// called, which only happens when SyncBackupManager is shut down and a
// syncing backend is to be created. Thus we guarantee that user data is not
// persisted until user is actually going to sync.
class SYNC_EXPORT_PRIVATE DeferredOnDiskDirectoryBackingStore
    : public DirectoryBackingStore {
 public:
  DeferredOnDiskDirectoryBackingStore(const std::string& dir_name,
                                      const base::FilePath& backing_filepath);
  virtual ~DeferredOnDiskDirectoryBackingStore();
  virtual DirOpenResult Load(
      Directory::MetahandlesMap* handles_map,
      JournalIndex* delete_journals,
      Directory::KernelLoadInfo* kernel_load_info) OVERRIDE;
  virtual bool SaveChanges(const Directory::SaveChangesSnapshot& snapshot)
      OVERRIDE;

 private:
  base::FilePath backing_filepath_;

  // Whether current DB is on disk.
  bool db_is_on_disk_;

  DISALLOW_COPY_AND_ASSIGN(DeferredOnDiskDirectoryBackingStore);
};

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_DEFERRED_ON_DISK_DIRECTORY_BACKING_STORE_H_
