// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/get_commit_ids.h"

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "sync/engine/syncer_util.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/syncable/nigori_util.h"
#include "sync/syncable/syncable_base_transaction.h"
#include "sync/syncable/syncable_util.h"
#include "sync/util/cryptographer.h"

using std::set;
using std::vector;

namespace syncer {

namespace {

// Forward-declare some helper functions.  This gives us more options for
// ordering the function defintions within this file.

// Filters |unsynced_handles| to remove all entries that do not belong to the
// specified |requested_types|, or are not eligible for a commit at this time.
void FilterUnreadyEntries(
    syncable::BaseTransaction* trans,
    ModelTypeSet requested_types,
    ModelTypeSet encrypted_types,
    bool passphrase_missing,
    const syncable::Directory::Metahandles& unsynced_handles,
    std::set<int64>* ready_unsynced_set);

// Given a set of commit metahandles that are ready for commit
// (|ready_unsynced_set|), sorts these into commit order and places up to
// |max_entries| of them in the output parameter |out|.
//
// See the header file for an explanation of commit ordering.
void OrderCommitIds(
    syncable::BaseTransaction* trans,
    size_t max_entries,
    const std::set<int64>& ready_unsynced_set,
    std::vector<int64>* out);

}  // namespace

void GetCommitIdsForType(
    syncable::BaseTransaction* trans,
    ModelType type,
    size_t max_entries,
    syncable::Directory::Metahandles* out) {
  syncable::Directory* dir = trans->directory();

  // Gather the full set of unsynced items and store it in the session. They
  // are not in the correct order for commit.
  std::set<int64> ready_unsynced_set;
  syncable::Directory::Metahandles all_unsynced_handles;
  GetUnsyncedEntries(trans, &all_unsynced_handles);

  ModelTypeSet encrypted_types;
  bool passphrase_missing = false;
  Cryptographer* cryptographer = dir->GetCryptographer(trans);
  if (cryptographer) {
    encrypted_types = dir->GetNigoriHandler()->GetEncryptedTypes(trans);
    passphrase_missing = cryptographer->has_pending_keys();
  };

  // We filter out all unready entries from the set of unsynced handles. This
  // new set of ready and unsynced items is then what we use to determine what
  // is a candidate for commit.  The caller is responsible for ensuring that no
  // throttled types are included among the requested_types.
  FilterUnreadyEntries(trans,
                       ModelTypeSet(type),
                       encrypted_types,
                       passphrase_missing,
                       all_unsynced_handles,
                       &ready_unsynced_set);

  OrderCommitIds(trans, max_entries, ready_unsynced_set, out);

  for (size_t i = 0; i < out->size(); i++) {
    DVLOG(1) << "Debug commit batch result:" << (*out)[i];
  }
}

namespace {

bool IsEntryInConflict(const syncable::Entry& entry) {
  if (entry.GetIsUnsynced() &&
      entry.GetServerVersion() > 0 &&
      (entry.GetServerVersion() > entry.GetBaseVersion())) {
    // The local and server versions don't match. The item must be in
    // conflict, so there's no point in attempting to commit.
    DCHECK(entry.GetIsUnappliedUpdate());
    DVLOG(1) << "Excluding entry from commit due to version mismatch "
             << entry;
    return true;
  }
  return false;
}

// Return true if this entry has any attachments that haven't yet been uploaded
// to the server.
bool HasAttachmentNotOnServer(const syncable::Entry& entry) {
  const sync_pb::AttachmentMetadata& metadata = entry.GetAttachmentMetadata();
  for (int i = 0; i < metadata.record_size(); ++i) {
    if (!metadata.record(i).is_on_server()) {
      return true;
    }
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
bool IsEntryReadyForCommit(ModelTypeSet requested_types,
                           ModelTypeSet encrypted_types,
                           bool passphrase_missing,
                           const syncable::Entry& entry) {
  DCHECK(entry.GetIsUnsynced());
  if (IsEntryInConflict(entry))
    return false;

  const ModelType type = entry.GetModelType();
  // We special case the nigori node because even though it is considered an
  // "encrypted type", not all nigori node changes require valid encryption
  // (ex: sync_tabs).
  if ((type != NIGORI) && encrypted_types.Has(type) &&
      (passphrase_missing ||
       syncable::EntryNeedsEncryption(encrypted_types, entry))) {
    // This entry requires encryption but is not properly encrypted (possibly
    // due to the cryptographer not being initialized or the user hasn't
    // provided the most recent passphrase).
    DVLOG(1) << "Excluding entry from commit due to lack of encryption "
             << entry;
    return false;
  }

  // Ignore it if it's not in our set of requested types.
  if (!requested_types.Has(type))
    return false;

  if (entry.GetIsDel() && !entry.GetId().ServerKnows()) {
    // New clients (following the resolution of crbug.com/125381) should not
    // create such items.  Old clients may have left some in the database
    // (crbug.com/132905), but we should now be cleaning them on startup.
    NOTREACHED() << "Found deleted and unsynced local item: " << entry;
    return false;
  }

  // Extra validity checks.
  syncable::Id id = entry.GetId();
  if (id == entry.GetParentId()) {
    CHECK(id.IsRoot()) << "Non-root item is self parenting." << entry;
    // If the root becomes unsynced it can cause us problems.
    NOTREACHED() << "Root item became unsynced " << entry;
    return false;
  }

  if (entry.IsRoot()) {
    NOTREACHED() << "Permanent item became unsynced " << entry;
    return false;
  }

  if (HasAttachmentNotOnServer(entry)) {
    // This entry is not ready to be sent to the server because it has one or
    // more attachments that have not yet been uploaded to the server.  The idea
    // here is avoid propagating an entry with dangling attachment references.
    return false;
  }

  DVLOG(2) << "Entry is ready for commit: " << entry;
  return true;
}

// Filters |unsynced_handles| to remove all entries that do not belong to the
// specified |requested_types|, or are not eligible for a commit at this time.
void FilterUnreadyEntries(
    syncable::BaseTransaction* trans,
    ModelTypeSet requested_types,
    ModelTypeSet encrypted_types,
    bool passphrase_missing,
    const syncable::Directory::Metahandles& unsynced_handles,
    std::set<int64>* ready_unsynced_set) {
  for (syncable::Directory::Metahandles::const_iterator iter =
       unsynced_handles.begin(); iter != unsynced_handles.end(); ++iter) {
    syncable::Entry entry(trans, syncable::GET_BY_HANDLE, *iter);
    // TODO(maniscalco): While we check if entry is ready to be committed, we
    // also need to check that all of its ancestors (parents, transitive) are
    // ready to be committed.  Once attachments can prevent an entry from being
    // committable, this method must ensure all ancestors are ready for commit
    // (bug 356273).
    if (IsEntryReadyForCommit(requested_types,
                              encrypted_types,
                              passphrase_missing,
                              entry)) {
      ready_unsynced_set->insert(*iter);
    }
  }
}

// This class helps to implement OrderCommitIds().  Its members track the
// progress of a traversal while its methods extend it.  It can return early if
// the traversal reaches the desired size before the full traversal is complete.
class Traversal {
 public:
  Traversal(
    syncable::BaseTransaction* trans,
    int64 max_entries,
    syncable::Directory::Metahandles* out);
  ~Traversal();

  // First step of traversal building.  Adds non-deleted items in order.
  void AddCreatesAndMoves(const std::set<int64>& ready_unsynced_set);

  // Second step of traverals building.  Appends deleted items.
  void AddDeletes(const std::set<int64>& ready_unsynced_set);

 private:
  // The following functions do not modify the traversal directly.  They return
  // their results in the |result| vector instead.
  bool AddUncommittedParentsAndTheirPredecessors(
      const std::set<int64>& ready_unsynced_set,
      const syncable::Entry& item,
      syncable::Directory::Metahandles* result) const;

  void TryAddItem(const std::set<int64>& ready_unsynced_set,
                  const syncable::Entry& item,
                  syncable::Directory::Metahandles* result) const;

  void AddItemThenPredecessors(
      const std::set<int64>& ready_unsynced_set,
      const syncable::Entry& item,
      syncable::Directory::Metahandles* result) const;

  void AddPredecessorsThenItem(
      const std::set<int64>& ready_unsynced_set,
      const syncable::Entry& item,
      syncable::Directory::Metahandles* result) const;

  // Returns true if we've collected enough items.
  bool IsFull() const;

  // Returns true if the specified handle is already in the traversal.
  bool HaveItem(int64 handle) const;

  // Adds the specified handles to the traversal.
  void AppendManyToTraversal(const syncable::Directory::Metahandles& handles);

  // Adds the specifed handle to the traversal.
  void AppendToTraversal(int64 handle);

  syncable::Directory::Metahandles* out_;
  std::set<int64> added_handles_;
  const size_t max_entries_;
  syncable::BaseTransaction* trans_;

  DISALLOW_COPY_AND_ASSIGN(Traversal);
};

Traversal::Traversal(
    syncable::BaseTransaction* trans,
    int64 max_entries,
    syncable::Directory::Metahandles* out)
  : out_(out),
    max_entries_(max_entries),
    trans_(trans) { }

Traversal::~Traversal() {}

bool Traversal::AddUncommittedParentsAndTheirPredecessors(
    const std::set<int64>& ready_unsynced_set,
    const syncable::Entry& item,
    syncable::Directory::Metahandles* result) const {
  syncable::Directory::Metahandles dependencies;
  syncable::Id parent_id = item.GetParentId();

  // Climb the tree adding entries leaf -> root.
  while (!parent_id.ServerKnows()) {
    syncable::Entry parent(trans_, syncable::GET_BY_ID, parent_id);
    CHECK(parent.good()) << "Bad user-only parent in item path.";
    int64 handle = parent.GetMetahandle();
    if (HaveItem(handle)) {
      // We've already added this parent (and therefore all of its parents).
      // We can return early.
      break;
    }
    if (IsEntryInConflict(parent)) {
      // We ignore all entries that are children of a conflicing item.  Return
      // false immediately to forget the traversal we've built up so far.
      DVLOG(1) << "Parent was in conflict, omitting " << item;
      return false;
    }
    AddItemThenPredecessors(ready_unsynced_set,
                            parent,
                            &dependencies);
    parent_id = parent.GetParentId();
  }

  // Reverse what we added to get the correct order.
  result->insert(result->end(), dependencies.rbegin(), dependencies.rend());
  return true;
}

// Adds the given item to the list if it is unsynced and ready for commit.
void Traversal::TryAddItem(const std::set<int64>& ready_unsynced_set,
                           const syncable::Entry& item,
                           syncable::Directory::Metahandles* result) const {
  DCHECK(item.GetIsUnsynced());
  int64 item_handle = item.GetMetahandle();
  if (ready_unsynced_set.count(item_handle) != 0) {
    result->push_back(item_handle);
  }
}

// Adds the given item, and all its unsynced predecessors.  The traversal will
// be cut short if any item along the traversal is not IS_UNSYNCED, or if we
// detect that this area of the tree has already been traversed.  Items that are
// not 'ready' for commit (see IsEntryReadyForCommit()) will not be added to the
// list, though they will not stop the traversal.
void Traversal::AddItemThenPredecessors(
    const std::set<int64>& ready_unsynced_set,
    const syncable::Entry& item,
    syncable::Directory::Metahandles* result) const {
  int64 item_handle = item.GetMetahandle();
  if (HaveItem(item_handle)) {
    // We've already added this item to the commit set, and so must have
    // already added the predecessors as well.
    return;
  }
  TryAddItem(ready_unsynced_set, item, result);
  if (item.GetIsDel())
    return;  // Deleted items have no predecessors.

  syncable::Id prev_id = item.GetPredecessorId();
  while (!prev_id.IsRoot()) {
    syncable::Entry prev(trans_, syncable::GET_BY_ID, prev_id);
    CHECK(prev.good()) << "Bad id when walking predecessors.";
    if (!prev.GetIsUnsynced()) {
      // We're interested in "runs" of unsynced items.  This item breaks
      // the streak, so we stop traversing.
      return;
    }
    int64 handle = prev.GetMetahandle();
    if (HaveItem(handle)) {
      // We've already added this item to the commit set, and so must have
      // already added the predecessors as well.
      return;
    }
    TryAddItem(ready_unsynced_set, prev, result);
    prev_id = prev.GetPredecessorId();
  }
}

// Same as AddItemThenPredecessor, but the traversal order will be reversed.
void Traversal::AddPredecessorsThenItem(
    const std::set<int64>& ready_unsynced_set,
    const syncable::Entry& item,
    syncable::Directory::Metahandles* result) const {
  syncable::Directory::Metahandles dependencies;
  AddItemThenPredecessors(ready_unsynced_set, item, &dependencies);

  // Reverse what we added to get the correct order.
  result->insert(result->end(), dependencies.rbegin(), dependencies.rend());
}

bool Traversal::IsFull() const {
  return out_->size() >= max_entries_;
}

bool Traversal::HaveItem(int64 handle) const {
  return added_handles_.find(handle) != added_handles_.end();
}

void Traversal::AppendManyToTraversal(
    const syncable::Directory::Metahandles& handles) {
  out_->insert(out_->end(), handles.begin(), handles.end());
  added_handles_.insert(handles.begin(), handles.end());
}

void Traversal::AppendToTraversal(int64 metahandle) {
  out_->push_back(metahandle);
  added_handles_.insert(metahandle);
}

void Traversal::AddCreatesAndMoves(
    const std::set<int64>& ready_unsynced_set) {
  // Add moves and creates, and prepend their uncommitted parents.
  for (std::set<int64>::const_iterator iter = ready_unsynced_set.begin();
       !IsFull() && iter != ready_unsynced_set.end(); ++iter) {
    int64 metahandle = *iter;
    if (HaveItem(metahandle))
      continue;

    syncable::Entry entry(trans_,
                          syncable::GET_BY_HANDLE,
                          metahandle);
    if (!entry.GetIsDel()) {
      // We only commit an item + its dependencies if it and all its
      // dependencies are not in conflict.
      syncable::Directory::Metahandles item_dependencies;
      if (AddUncommittedParentsAndTheirPredecessors(
              ready_unsynced_set,
              entry,
              &item_dependencies)) {
        AddPredecessorsThenItem(ready_unsynced_set,
                                entry,
                                &item_dependencies);
        AppendManyToTraversal(item_dependencies);
      }
    }
  }

  // It's possible that we overcommitted while trying to expand dependent
  // items.  If so, truncate the set down to the allowed size.
  if (out_->size() > max_entries_)
    out_->resize(max_entries_);
}

void Traversal::AddDeletes(
    const std::set<int64>& ready_unsynced_set) {
  set<syncable::Id> legal_delete_parents;

  for (std::set<int64>::const_iterator iter = ready_unsynced_set.begin();
       !IsFull() && iter != ready_unsynced_set.end(); ++iter) {
    int64 metahandle = *iter;
    if (HaveItem(metahandle))
      continue;

    syncable::Entry entry(trans_, syncable::GET_BY_HANDLE,
                          metahandle);

    if (entry.GetIsDel()) {
      syncable::Entry parent(trans_, syncable::GET_BY_ID,
                             entry.GetParentId());
      // If the parent is deleted and unsynced, then any children of that
      // parent don't need to be added to the delete queue.
      //
      // Note: the parent could be synced if there was an update deleting a
      // folder when we had a deleted all items in it.
      // We may get more updates, or we may want to delete the entry.
      if (parent.good() && parent.GetIsDel() && parent.GetIsUnsynced()) {
        // However, if an entry is moved, these rules can apply differently.
        //
        // If the entry was moved, then the destination parent was deleted,
        // then we'll miss it in the roll up. We have to add it in manually.
        // TODO(chron): Unit test for move / delete cases:
        // Case 1: Locally moved, then parent deleted
        // Case 2: Server moved, then locally issue recursive delete.
        if (entry.GetId().ServerKnows() &&
            entry.GetParentId() != entry.GetServerParentId()) {
          DVLOG(1) << "Inserting moved and deleted entry, will be missed by "
                   << "delete roll." << entry.GetId();

          AppendToTraversal(metahandle);
        }

        // Skip this entry since it's a child of a parent that will be
        // deleted. The server will unroll the delete and delete the
        // child as well.
        continue;
      }

      legal_delete_parents.insert(entry.GetParentId());
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
       !IsFull() && iter != ready_unsynced_set.end(); ++iter) {
    int64 metahandle = *iter;
    if (HaveItem(metahandle))
      continue;
    syncable::Entry entry(trans_, syncable::GET_BY_HANDLE, metahandle);
    if (entry.GetIsDel()) {
      syncable::Id parent_id = entry.GetParentId();
      if (legal_delete_parents.count(parent_id)) {
        AppendToTraversal(metahandle);
      }
    }
  }
}

void OrderCommitIds(
    syncable::BaseTransaction* trans,
    size_t max_entries,
    const std::set<int64>& ready_unsynced_set,
    syncable::Directory::Metahandles* out) {
  // Commits follow these rules:
  // 1. Moves or creates are preceded by needed folder creates, from
  //    root to leaf.  For folders whose contents are ordered, moves
  //    and creates appear in order.
  // 2. Moves/Creates before deletes.
  // 3. Deletes, collapsed.
  // We commit deleted moves under deleted items as moves when collapsing
  // delete trees.

  Traversal traversal(trans, max_entries, out);

  // Add moves and creates, and prepend their uncommitted parents.
  traversal.AddCreatesAndMoves(ready_unsynced_set);

  // Add all deletes.
  traversal.AddDeletes(ready_unsynced_set);
}

}  // namespace

}  // namespace syncer
