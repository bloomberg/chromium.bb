// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/on_disk_directory_backing_store.h"

#include "base/logging.h"

namespace syncer {
namespace syncable {

OnDiskDirectoryBackingStore::OnDiskDirectoryBackingStore(
    const std::string& dir_name, const FilePath& backing_filepath)
    : DirectoryBackingStore(dir_name),
      backing_filepath_(backing_filepath) {
  db_->set_exclusive_locking();
  db_->set_page_size(4096);
}

DirOpenResult OnDiskDirectoryBackingStore::Load(
    MetahandlesIndex* entry_bucket,
    Directory::KernelLoadInfo* kernel_load_info) {
  DCHECK(CalledOnValidThread());
  if (!db_->is_open()) {
    if (!db_->Open(backing_filepath_))
      return FAILED_OPEN_DATABASE;
  }

  if (!InitializeTables())
    return FAILED_OPEN_DATABASE;

  if (!DropDeletedEntries())
    return FAILED_DATABASE_CORRUPT;
  if (!LoadEntries(entry_bucket))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadInfo(kernel_load_info))
    return FAILED_DATABASE_CORRUPT;

  return OPENED;
}

}  // namespace syncable
}  // namespace syncer
