// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/syncable_util.h"

#include "base/base64.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_id.h"
#include "sync/syncable/syncable_write_transaction.h"

namespace syncer {
namespace syncable {

// Returns the number of unsynced entries.
int GetUnsyncedEntries(BaseTransaction* trans,
                       std::vector<int64> *handles) {
  trans->directory()->GetUnsyncedMetaHandles(trans, handles);
  DVLOG_IF(1, !handles->empty()) << "Have " << handles->size()
                                 << " unsynced items.";
  return handles->size();
}

bool IsLegalNewParent(BaseTransaction* trans, const Id& entry_id,
                      const Id& new_parent_id) {
  if (entry_id.IsRoot())
    return false;
  // we have to ensure that the entry is not an ancestor of the new parent.
  Id ancestor_id = new_parent_id;
  while (!ancestor_id.IsRoot()) {
    if (entry_id == ancestor_id)
      return false;
    Entry new_parent(trans, GET_BY_ID, ancestor_id);
    if (!SyncAssert(new_parent.good(),
                    FROM_HERE,
                    "Invalid new parent",
                    trans))
      return false;
    ancestor_id = new_parent.Get(PARENT_ID);
  }
  return true;
}

void ChangeEntryIDAndUpdateChildren(
    WriteTransaction* trans,
    MutableEntry* entry,
    const Id& new_id) {
  Id old_id = entry->Get(ID);
  if (!entry->Put(ID, new_id)) {
    Entry old_entry(trans, GET_BY_ID, new_id);
    CHECK(old_entry.good());
    LOG(FATAL) << "Attempt to change ID to " << new_id
               << " conflicts with existing entry.\n\n"
               << *entry << "\n\n" << old_entry;
  }
  if (entry->Get(IS_DIR)) {
    // Get all child entries of the old id.
    Directory::ChildHandles children;
    trans->directory()->GetChildHandlesById(trans, old_id, &children);
    Directory::ChildHandles::iterator i = children.begin();
    while (i != children.end()) {
      MutableEntry child_entry(trans, GET_BY_HANDLE, *i++);
      CHECK(child_entry.good());
      // Use the unchecked setter here to avoid touching the child's NEXT_ID
      // and PREV_ID fields (which Put(PARENT_ID) would normally do to
      // maintain linked-list invariants).  In this case, NEXT_ID and PREV_ID
      // among the children will be valid after the loop, since we update all
      // the children at once.
      child_entry.PutParentIdPropertyOnly(new_id);
    }
  }
  // Update Id references on the previous and next nodes in the sibling
  // order.  Do this by reinserting into the linked list; the first
  // step in PutPredecessor is to Unlink from the existing order, which
  // will overwrite the stale Id value from the adjacent nodes.
  if (entry->GetPredecessorId() == entry->GetSuccessorId() &&
      entry->GetPredecessorId() == old_id) {
    // We just need a shallow update to |entry|'s fields since it is already
    // self looped.
    entry->Put(NEXT_ID, new_id);
    entry->Put(PREV_ID, new_id);
  } else {
    entry->PutPredecessor(entry->GetPredecessorId());
  }
}

// Function to handle runtime failures on syncable code. Rather than crashing,
// if the |condition| is false the following will happen:
// 1. Sets unrecoverable error on transaction.
// 2. Returns false.
bool SyncAssert(bool condition,
                const tracked_objects::Location& location,
                const char* msg,
                BaseTransaction* trans) {
  if (!condition) {
    trans->OnUnrecoverableError(location, msg);
    return false;
  }
  return true;
}

std::string GenerateSyncableHash(
    ModelType model_type, const std::string& client_tag) {
  // Blank PB with just the field in it has termination symbol,
  // handy for delimiter.
  sync_pb::EntitySpecifics serialized_type;
  AddDefaultFieldValue(model_type, &serialized_type);
  std::string hash_input;
  serialized_type.AppendToString(&hash_input);
  hash_input.append(client_tag);

  std::string encode_output;
  CHECK(base::Base64Encode(base::SHA1HashString(hash_input), &encode_output));
  return encode_output;
}

}  // namespace syncable
}  // namespace syncer
