// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/dom_storage/session_storage_database_adapter.h"

#include "webkit/dom_storage/session_storage_database.h"

namespace dom_storage {

SessionStorageDatabaseAdapter::SessionStorageDatabaseAdapter(
    SessionStorageDatabase* db,
    const std::string& permanent_namespace_id,
    const GURL& origin)
    : db_(db),
      permanent_namespace_id_(permanent_namespace_id),
      origin_(origin) {
}

SessionStorageDatabaseAdapter::~SessionStorageDatabaseAdapter() { }

void SessionStorageDatabaseAdapter::ReadAllValues(ValuesMap* result) {
  db_->ReadAreaValues(permanent_namespace_id_, origin_, result);
}

bool SessionStorageDatabaseAdapter::CommitChanges(
    bool clear_all_first, const ValuesMap& changes) {
  return db_->CommitAreaChanges(permanent_namespace_id_, origin_,
                                clear_all_first, changes);
}

}  // namespace dom_storage
