// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_SYNCABLE_SYNCABLE_UTIL_H_
#define SYNC_SYNCABLE_SYNCABLE_UTIL_H_

namespace tracked_objects {
class Location;
}

namespace syncable {

class BaseTransaction;
class WriteTransaction;
class MutableEntry;
class Id;

void ChangeEntryIDAndUpdateChildren(syncable::WriteTransaction* trans,
                                    syncable::MutableEntry* entry,
                                    const syncable::Id& new_id);

bool IsLegalNewParent(BaseTransaction* trans, const Id& id, const Id& parentid);

bool SyncAssert(bool condition,
                const tracked_objects::Location& location,
                const char* msg,
                BaseTransaction* trans);
}  // namespace syncable

#endif  // SYNC_SYNCABLE_SYNCABLE_UTIL_H_
