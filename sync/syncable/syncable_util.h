// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_UTIL_H_
#define SYNC_SYNCABLE_SYNCABLE_UTIL_H_

#include <vector>

#include "base/basictypes.h"

namespace tracked_objects {
class Location;
}

namespace syncer {
namespace syncable {

class BaseTransaction;
class WriteTransaction;
class MutableEntry;
class Id;

void ChangeEntryIDAndUpdateChildren(WriteTransaction* trans,
                                    MutableEntry* entry,
                                    const Id& new_id);

bool IsLegalNewParent(BaseTransaction* trans, const Id& id, const Id& parentid);

bool SyncAssert(bool condition,
                const tracked_objects::Location& location,
                const char* msg,
                BaseTransaction* trans);

int GetUnsyncedEntries(BaseTransaction* trans,
                       std::vector<int64> *handles);

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_SYNCABLE_UTIL_H_
