// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_UTIL_H_
#define SYNC_SYNCABLE_SYNCABLE_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"

namespace tracked_objects {
class Location;
}

namespace syncer {
namespace syncable {

class BaseTransaction;
class BaseWriteTransaction;
class ModelNeutralMutableEntry;
class Id;

SYNC_EXPORT_PRIVATE void ChangeEntryIDAndUpdateChildren(
    BaseWriteTransaction* trans,
    ModelNeutralMutableEntry* entry,
    const Id& new_id);

SYNC_EXPORT_PRIVATE bool IsLegalNewParent(BaseTransaction* trans,
                                          const Id& id,
                                          const Id& parentid);

bool SyncAssert(bool condition,
                const tracked_objects::Location& location,
                const char* msg,
                BaseTransaction* trans);

SYNC_EXPORT_PRIVATE int GetUnsyncedEntries(BaseTransaction* trans,
                                           std::vector<int64> *handles);

// Generates a fixed-length tag for the given string under the given model_type.
SYNC_EXPORT_PRIVATE std::string GenerateSyncableHash(
    ModelType model_type, const std::string& client_tag);

// A helper for generating the bookmark type's tag.  This is required in more
// than one place, so we define the algorithm here to make sure the
// implementation is consistent.
SYNC_EXPORT_PRIVATE std::string GenerateSyncableBookmarkHash(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id);

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_SYNCABLE_UTIL_H_
