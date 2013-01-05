// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/test/test_directory_backing_store.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace syncer {
namespace syncable {

TestDirectoryBackingStore::TestDirectoryBackingStore(
    const std::string& dir_name, sql::Connection* db)
  : DirectoryBackingStore(dir_name, db) {
}

TestDirectoryBackingStore::~TestDirectoryBackingStore() {
  // This variant of the DirectoryBackingStore does not own its connection, so
  // we take care to not delete it here.
  ignore_result(db_.release());
}

DirOpenResult TestDirectoryBackingStore::Load(
    MetahandlesIndex* entry_bucket,
    JournalIndex* delete_journals,
    Directory::KernelLoadInfo* kernel_load_info) {
  DCHECK(db_->is_open());

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
