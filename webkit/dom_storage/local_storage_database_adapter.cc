// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/local_storage_database_adapter.h"

#include "base/file_util.h"
#include "webkit/dom_storage/dom_storage_database.h"

namespace dom_storage {

LocalStorageDatabaseAdapter::LocalStorageDatabaseAdapter(
    const base::FilePath& path)
    : db_(new DomStorageDatabase(path)) {
}

LocalStorageDatabaseAdapter::~LocalStorageDatabaseAdapter() { }

void LocalStorageDatabaseAdapter::ReadAllValues(ValuesMap* result) {
  db_->ReadAllValues(result);
}

bool LocalStorageDatabaseAdapter::CommitChanges(
    bool clear_all_first, const ValuesMap& changes) {
  return db_->CommitChanges(clear_all_first, changes);
}

void LocalStorageDatabaseAdapter::DeleteFiles() {
  file_util::Delete(db_->file_path(), false);
  file_util::Delete(
      DomStorageDatabase::GetJournalFilePath(db_->file_path()), false);
}

void LocalStorageDatabaseAdapter::Reset() {
  db_.reset(new DomStorageDatabase(db_->file_path()));
}

LocalStorageDatabaseAdapter::LocalStorageDatabaseAdapter()
    : db_(new DomStorageDatabase()) {
}

}  // namespace dom_storage
