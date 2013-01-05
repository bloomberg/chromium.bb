// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/in_memory_directory_backing_store.h"

namespace syncer {
namespace syncable {

InMemoryDirectoryBackingStore::InMemoryDirectoryBackingStore(
    const std::string& dir_name) : DirectoryBackingStore(dir_name) {
}

DirOpenResult InMemoryDirectoryBackingStore::Load(
    MetahandlesIndex* entry_bucket,
    JournalIndex* delete_journals,
    Directory::KernelLoadInfo* kernel_load_info) {
  if (!db_->is_open()) {
    if (!db_->OpenInMemory())
      return FAILED_OPEN_DATABASE;
  }

  if (!InitializeTables())
    return FAILED_OPEN_DATABASE;

  if (!DropDeletedEntries())
    return FAILED_DATABASE_CORRUPT;
  if (!LoadEntries(entry_bucket))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadDeleteJournals(delete_journals))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadInfo(kernel_load_info))
    return FAILED_DATABASE_CORRUPT;
  if (!VerifyReferenceIntegrity(*entry_bucket))
    return FAILED_DATABASE_CORRUPT;

  return OPENED;
}

}  // namespace syncable
}  // namespace syncer
