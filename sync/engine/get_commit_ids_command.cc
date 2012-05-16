// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/get_commit_ids_command.h"

#include <set>
#include <utility>
#include <vector>

#include "sync/engine/nigori_util.h"
#include "sync/engine/syncer_util.h"
#include "sync/syncable/syncable.h"
#include "sync/util/cryptographer.h"

using std::set;
using std::vector;

namespace browser_sync {

using sessions::OrderedCommitSet;
using sessions::SyncSession;
using sessions::StatusController;

GetCommitIdsCommand::GetCommitIdsCommand(int commit_batch_size)
    : requested_commit_batch_size_(commit_batch_size) {}

GetCommitIdsCommand::~GetCommitIdsCommand() {}

SyncerError GetCommitIdsCommand::ExecuteImpl(SyncSession* session) {
  // Gather the full set of unsynced items and store it in the session. They
  // are not in the correct order for commit.
  std::set<int64> ready_unsynced_set;
  syncable::Directory::UnsyncedMetaHandles all_unsynced_handles;
  SyncerUtil::GetUnsyncedEntries(session->write_transaction(),
                                 &all_unsynced_handles);

  syncable::ModelTypeSet encrypted_types;
  bool passphrase_missing = false;
  Cryptographer* cryptographer =
      session->context()->
      directory()->GetCryptographer(session->write_transaction());
  if (cryptographer) {
    encrypted_types = cryptographer->GetEncryptedTypes();
    passphrase_missing = cryptographer->has_pending_keys();
  };

  const syncable::ModelTypeSet throttled_types =
       session->context()->GetThrottledTypes();
  // We filter out all unready entries from the set of unsynced handles. This
  // new set of ready and unsynced items (which excludes throttled items as
  // well) is then what we use to determine what is a candidate for commit.
  FilterUnreadyEntries(session->write_transaction(),
                       throttled_types,
                       encrypted_types,
                       passphrase_missing,
                       all_unsynced_handles,
                       &ready_unsynced_set);

  BuildCommitIds(session->write_transaction(),
                 session->routing_info(),
                 ready_unsynced_set);

  StatusController* status = session->mutable_status_controller();
  syncable::Directory::UnsyncedMetaHandles ready_unsynced_vector(
      ready_unsynced_set.begin(), ready_unsynced_set.end());
  status->set_unsynced_handles(ready_unsynced_vector);
  const vector<syncable::Id>& verified_commit_ids =
      ordered_commit_set_->GetAllCommitIds();

  for (size_t i = 0; i < verified_commit_ids.size(); i++)
    DVLOG(1) << "Debug commit batch result:" << verified_commit_ids[i];

  status->set_commit_set(*ordered_commit_set_.get());
  return SYNCER_OK;
}

namespace {

bool IsEntryInConflict(const syncable::Entry& entry) {
  if (entry.Get(syncable::IS_UNSYNCED) &&
      entry.Get(syncable::SERVER_VERSION) > 0 &&
      (entry.Get(syncable::SERVER_VERSION) >
       entry.Get(syncable::BASE_VERSION))) {
    // The local and server versions don't match. The item must be in
    // conflict, so there's no point in attempting to commit.
    DCHECK(entry.Get(syncable::IS_UNAPPLIED_UPDATE));
    DVLOG(1) << "Excluding entry from commit due to version mismatch "
             << entry;
    return true;
  }
  return false;
}

// An entry is not considered ready for commit if any are true:
// 1. It's in conflict.
// 2. It requires encryption (either the type is encrypted but a passphrase
//    is missing from the cryptographer, or the entry itself wasn't properly
//    encrypted).
// 3. It's type is currently throttled.
// 4. It's a delete but has not been committed.
bool IsEntryReadyForCommit(syncable::ModelTypeSet throttled_types,
                           syncable::ModelTypeSet encrypted_types,
                           bool passphrase_missing,
                           const syncable::Entry& entry) {
  DCHECK(entry.Get(syncable::IS_UNSYNCED));
  if (IsEntryInConflict(entry))
    return false;

  const syncable::ModelType type = entry.GetModelType();
  // We special case the nigori node because even though it is considered an
  // "encrypted type", not all nigori node changes require valid encryption
  // (ex: sync_tabs).
  if ((type != syncable::NIGORI) &&
      encrypted_types.Has(type) &&
      (passphrase_missing ||
       syncable::EntryNeedsEncryption(encrypted_types, entry))) {
    // This entry requires encryption but is not properly encrypted (possibly
    // due to the cryptographer not being initialized or the user hasn't
    // provided the most recent passphrase).
    DVLOG(1) << "Excluding entry from commit due to lack of encryption "
             << entry;
    return false;
  }

  // Look at the throttled types.
  if (throttled_types.Has(type))
    return false;

  // Extra validity checks.
  syncable::Id id = entry.Get(syncable::ID);
  if (id == entry.Get(syncable::PARENT_ID)) {
    CHECK(id.IsRoot()) << "Non-root item is self parenting." << entry;
    // If the root becomes unsynced it can cause us problems.
    NOTREACHED() << "Root item became unsynced " << entry;
    return false;
  }

  if (entry.IsRoot()) {
    NOTREACHED() << "Permanent item became unsynced " << entry;
    return false;
  }

  DVLOG(2) << "Entry is ready for commit: " << entry;
  return true;
}

}  // namespace

void GetCommitIdsCommand::FilterUnreadyEntries(
    syncable::BaseTransaction* trans,
    syncable::ModelTypeSet throttled_types,
    syncable::ModelTypeSet encrypted_types,
    bool passphrase_missing,
    const syncable::Directory::UnsyncedMetaHandles& unsynced_handles,
    std::set<int64>* ready_unsynced_set) {
  for (syncable::Directory::UnsyncedMetaHandles::const_iterator iter =
           unsynced_handles.begin(); iter != unsynced_handles.end(); ++iter) {
    syncable::Entry entry(trans, syncable::GET_BY_HANDLE, *iter);
    if (IsEntryReadyForCommit(throttled_types,
                              encrypted_types,
                              passphrase_missing,
                              entry)) {
      ready_unsynced_set->insert(*iter);
    }
  }
}

bool GetCommitIdsCommand::AddUncommittedParentsAndTheirPredecessors(
    syncable::BaseTransaction* trans,
    const ModelSafeRoutingInfo& routes,
    const std::set<int64>& ready_unsynced_set,
    const syncable::Entry& item,
    sessions::OrderedCommitSet* result) const {
  OrderedCommitSet item_dependencies(routes);
  syncable::Id parent_id = item.Get(syncable::PARENT_ID);

  // Climb the tree adding entries leaf -> root.
  while (!parent_id.ServerKnows()) {
    syncable::Entry parent(trans, syncable::GET_BY_ID, parent_id);
    CHECK(parent.good()) << "Bad user-only parent in item path.";
    int64 handle = parent.Get(syncable::META_HANDLE);
    if (ordered_commit_set_->HaveCommitItem(handle)) {
      // We've already added this parent (and therefore all of its parents).
      // We can return early.
      break;
    }
    if (!AddItemThenPredecessors(trans, ready_unsynced_set, parent,
                                 &item_dependencies)) {
      // There was a parent/predecessor in conflict. We return without adding
      // anything to |ordered_commit_set_|.
      DVLOG(1) << "Parent or parent's predecessor was in conflict, omitting "
               << item;
      return false;
    }
    parent_id = parent.Get(syncable::PARENT_ID);
  }

  // Reverse what we added to get the correct order.
  result->AppendReverse(item_dependencies);
  return true;
}

bool GetCommitIdsCommand::AddItem(const std::set<int64>& ready_unsynced_set,
                                  const syncable::Entry& item,
                                  OrderedCommitSet* result) const {
  DCHECK(item.Get(syncable::IS_UNSYNCED));
  // An item in conflict means that dependent items (successors and children)
  // cannot be added either.
  if (IsEntryInConflict(item))
    return false;
  int64 item_handle = item.Get(syncable::META_HANDLE);
  if (ready_unsynced_set.count(item_handle) == 0) {
    // It's not in conflict, but not ready for commit. Just return true without
    // adding it to the commit set.
    return true;
  }
  result->AddCommitItem(item_handle, item.Get(syncable::ID),
                        item.GetModelType());
  return true;
}

bool GetCommitIdsCommand::AddItemThenPredecessors(
    syncable::BaseTransaction* trans,
    const std::set<int64>& ready_unsynced_set,
    const syncable::Entry& item,
    OrderedCommitSet* result) const {
  int64 item_handle = item.Get(syncable::META_HANDLE);
  if (ordered_commit_set_->HaveCommitItem(item_handle)) {
    // We've already added this item to the commit set, and so must have
    // already added the predecessors as well.
    return true;
  }
  if (!AddItem(ready_unsynced_set, item, result))
    return false;  // Item is in conflict.
  if (item.Get(syncable::IS_DEL))
    return true;  // Deleted items have no predecessors.

  syncable::Id prev_id = item.Get(syncable::PREV_ID);
  while (!prev_id.IsRoot()) {
    syncable::Entry prev(trans, syncable::GET_BY_ID, prev_id);
    CHECK(prev.good()) << "Bad id when walking predecessors.";
    if (!prev.Get(syncable::IS_UNSYNCED))
      break;
    int64 handle = prev.Get(syncable::META_HANDLE);
    if (ordered_commit_set_->HaveCommitItem(handle)) {
      // We've already added this item to the commit set, and so must have
      // already added the predecessors as well.
      return true;
    }
    if (!AddItem(ready_unsynced_set, prev, result))
      return false;  // Item is in conflict.
    prev_id = prev.Get(syncable::PREV_ID);
  }
  return true;
}

bool GetCommitIdsCommand::AddPredecessorsThenItem(
    syncable::BaseTransaction* trans,
    const ModelSafeRoutingInfo& routes,
    const std::set<int64>& ready_unsynced_set,
    const syncable::Entry& item,
    OrderedCommitSet* result) const {
  OrderedCommitSet item_dependencies(routes);
  if (!AddItemThenPredecessors(trans, ready_unsynced_set, item,
                               &item_dependencies)) {
    // Either the item or its predecessors are in conflict, so don't add any
    // items to the commit set.
    DVLOG(1) << "Predecessor was in conflict, omitting " << item;
    return false;
  }

  // Reverse what we added to get the correct order.
  result->AppendReverse(item_dependencies);
  return true;
}

bool GetCommitIdsCommand::IsCommitBatchFull() const {
  return ordered_commit_set_->Size() >= requested_commit_batch_size_;
}

void GetCommitIdsCommand::AddCreatesAndMoves(
    syncable::WriteTransaction* write_transaction,
    const ModelSafeRoutingInfo& routes,
    const std::set<int64>& ready_unsynced_set) {
  // Add moves and creates, and prepend their uncommitted parents.
  for (std::set<int64>::const_iterator iter = ready_unsynced_set.begin();
       !IsCommitBatchFull() && iter != ready_unsynced_set.end(); ++iter) {
    int64 metahandle = *iter;
    if (ordered_commit_set_->HaveCommitItem(metahandle))
      continue;

    syncable::Entry entry(write_transaction,
                          syncable::GET_BY_HANDLE,
                          metahandle);
    if (!entry.Get(syncable::IS_DEL)) {
      // We only commit an item + its dependencies if it and all its
      // dependencies are not in conflict.
      OrderedCommitSet item_dependencies(routes);
      if (AddUncommittedParentsAndTheirPredecessors(
              write_transaction,
              routes,
              ready_unsynced_set,
              entry,
              &item_dependencies) &&
          AddPredecessorsThenItem(write_transaction,
                                  routes,
                                  ready_unsynced_set,
                                  entry,
                                  &item_dependencies)) {
        ordered_commit_set_->Append(item_dependencies);
      }
    }
  }

  // It's possible that we overcommitted while trying to expand dependent
  // items.  If so, truncate the set down to the allowed size.
  ordered_commit_set_->Truncate(requested_commit_batch_size_);
}

void GetCommitIdsCommand::AddDeletes(
    syncable::WriteTransaction* write_transaction,
    const std::set<int64>& ready_unsynced_set) {
  set<syncable::Id> legal_delete_parents;

  for (std::set<int64>::const_iterator iter = ready_unsynced_set.begin();
       !IsCommitBatchFull() && iter != ready_unsynced_set.end(); ++iter) {
    int64 metahandle = *iter;
    if (ordered_commit_set_->HaveCommitItem(metahandle))
      continue;

    syncable::Entry entry(write_transaction, syncable::GET_BY_HANDLE,
                          metahandle);

    if (entry.Get(syncable::IS_DEL)) {
      syncable::Entry parent(write_transaction, syncable::GET_BY_ID,
                             entry.Get(syncable::PARENT_ID));
      // If the parent is deleted and unsynced, then any children of that
      // parent don't need to be added to the delete queue.
      //
      // Note: the parent could be synced if there was an update deleting a
      // folder when we had a deleted all items in it.
      // We may get more updates, or we may want to delete the entry.
      if (parent.good() &&
          parent.Get(syncable::IS_DEL) &&
          parent.Get(syncable::IS_UNSYNCED)) {
        // However, if an entry is moved, these rules can apply differently.
        //
        // If the entry was moved, then the destination parent was deleted,
        // then we'll miss it in the roll up. We have to add it in manually.
        // TODO(chron): Unit test for move / delete cases:
        // Case 1: Locally moved, then parent deleted
        // Case 2: Server moved, then locally issue recursive delete.
        if (entry.Get(syncable::ID).ServerKnows() &&
            entry.Get(syncable::PARENT_ID) !=
                entry.Get(syncable::SERVER_PARENT_ID)) {
          DVLOG(1) << "Inserting moved and deleted entry, will be missed by "
                   << "delete roll." << entry.Get(syncable::ID);

          ordered_commit_set_->AddCommitItem(metahandle,
              entry.Get(syncable::ID),
              entry.GetModelType());
        }

        // Skip this entry since it's a child of a parent that will be
        // deleted. The server will unroll the delete and delete the
        // child as well.
        continue;
      }

      legal_delete_parents.insert(entry.Get(syncable::PARENT_ID));
    }
  }

  // We could store all the potential entries with a particular parent during
  // the above scan, but instead we rescan here. This is less efficient, but
  // we're dropping memory alloc/dealloc in favor of linear scans of recently
  // examined entries.
  //
  // Scan through the UnsyncedMetaHandles again. If we have a deleted
  // entry, then check if the parent is in legal_delete_parents.
  //
  // Parent being in legal_delete_parents means for the child:
  //   a recursive delete is not currently happening (no recent deletes in same
  //     folder)
  //   parent did expect at least one old deleted child
  //   parent was not deleted
  for (std::set<int64>::const_iterator iter = ready_unsynced_set.begin();
       !IsCommitBatchFull() && iter != ready_unsynced_set.end(); ++iter) {
    int64 metahandle = *iter;
    if (ordered_commit_set_->HaveCommitItem(metahandle))
      continue;
    syncable::MutableEntry entry(write_transaction, syncable::GET_BY_HANDLE,
                                 metahandle);
    if (entry.Get(syncable::IS_DEL)) {
      syncable::Id parent_id = entry.Get(syncable::PARENT_ID);
      if (legal_delete_parents.count(parent_id)) {
        ordered_commit_set_->AddCommitItem(metahandle, entry.Get(syncable::ID),
            entry.GetModelType());
      }
    }
  }
}

void GetCommitIdsCommand::BuildCommitIds(
    syncable::WriteTransaction* write_transaction,
    const ModelSafeRoutingInfo& routes,
    const std::set<int64>& ready_unsynced_set) {
  ordered_commit_set_.reset(new OrderedCommitSet(routes));
  // Commits follow these rules:
  // 1. Moves or creates are preceded by needed folder creates, from
  //    root to leaf.  For folders whose contents are ordered, moves
  //    and creates appear in order.
  // 2. Moves/Creates before deletes.
  // 3. Deletes, collapsed.
  // We commit deleted moves under deleted items as moves when collapsing
  // delete trees.

  // Add moves and creates, and prepend their uncommitted parents.
  AddCreatesAndMoves(write_transaction, routes, ready_unsynced_set);

  // Add all deletes.
  AddDeletes(write_transaction, ready_unsynced_set);
}

}  // namespace browser_sync
