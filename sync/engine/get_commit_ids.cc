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

  bool AddDeletedParents(const std::set<int64>& ready_unsynced_set,
                         const syncable::Entry& item,
                         const syncable::Directory::Metahandles& traversed,
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

// Traverses the tree from bottom to top, adding the deleted parents of the
// given |item|.  Stops traversing if it encounters a non-deleted node, or
// a node that was already listed in the |traversed| list.  Returns an error
// (false) if a node along the traversal is in a conflict state.
//
// The result list is reversed before it is returned, so the resulting
// traversal is in top to bottom order.  Also note that this function appends
// to the result list without clearing it.
bool Traversal::AddDeletedParents(
    const std::set<int64>& ready_unsynced_set,
    const syncable::Entry& item,
    const syncable::Directory::Metahandles& traversed,
    syncable::Directory::Metahandles* result) const {
  syncable::Directory::Metahandles dependencies;
  syncable::Id parent_id = item.GetParentId();

  // Climb the tree adding entries leaf -> root.
  while (!parent_id.IsRoot()) {
    syncable::Entry parent(trans_, syncable::GET_BY_ID, parent_id);

    if (!parent.good()) {
      // This is valid because the parent could have gone away a long time ago
      //
      // Consider the case where a folder is server-unknown and locally
      // deleted, and has a child that is server-known, deleted, and unsynced.
      // The parent could be dropped from memory at any time, but its child
      // needs to be committed first.
      break;
    }
    int64 handle = parent.GetMetahandle();
    if (!parent.GetIsUnsynced()) {
      // In some rare cases, our parent can be both deleted and unsynced.
      // (ie. the server-unknown parent case).
      break;
    }
    if (!parent.GetIsDel()) {
      // We're not intersted in non-deleted parents.
      break;
    }
    if (std::find(traversed.begin(), traversed.end(), handle) !=
        traversed.end()) {
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
    TryAddItem(ready_unsynced_set, parent, &dependencies);
    parent_id = parent.GetParentId();
  }

  // Reverse what we added to get the correct order.
  result->insert(result->end(), dependencies.rbegin(), dependencies.rend());
  return true;
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

void Traversal::AddDeletes(const std::set<int64>& ready_unsynced_set) {
  syncable::Directory::Metahandles deletion_list;

  for (std::set<int64>::const_iterator iter = ready_unsynced_set.begin();
       !IsFull() && iter != ready_unsynced_set.end(); ++iter) {
    int64 metahandle = *iter;

    if (HaveItem(metahandle))
      continue;

    if (std::find(deletion_list.begin(), deletion_list.end(), metahandle) !=
        deletion_list.end()) {
      continue;
    }

    syncable::Entry entry(trans_, syncable::GET_BY_HANDLE,
                          metahandle);

    if (entry.GetIsDel()) {
      syncable::Directory::Metahandles parents;
      if (AddDeletedParents(
              ready_unsynced_set, entry, deletion_list, &parents)) {
        // Append parents and chilren in top to bottom order.
        deletion_list.insert(deletion_list.end(),
                             parents.begin(),
                             parents.end());
        deletion_list.push_back(metahandle);
      }
    }

    if (deletion_list.size() + out_->size() > max_entries_)
      break;
  }

  // We've been gathering deletions in top to bottom order.  Now we reverse the
  // order as we prepare the list.
  std::reverse(deletion_list.begin(), deletion_list.end());
  AppendManyToTraversal(deletion_list);

  // It's possible that we overcommitted while trying to expand dependent
  // items.  If so, truncate the set down to the allowed size.
  if (out_->size() > max_entries_)
    out_->resize(max_entries_);
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
