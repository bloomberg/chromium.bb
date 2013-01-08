// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/conflict_resolver.h"

#include <list>
#include <set>
#include <string>

#include "base/metrics/histogram.h"
#include "sync/engine/conflict_util.h"
#include "sync/engine/syncer_util.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/util/cryptographer.h"

using std::list;
using std::set;

namespace syncer {

using sessions::StatusController;
using syncable::Directory;
using syncable::Entry;
using syncable::Id;
using syncable::MutableEntry;
using syncable::WriteTransaction;

ConflictResolver::ConflictResolver() {
}

ConflictResolver::~ConflictResolver() {
}

void ConflictResolver::ProcessSimpleConflict(WriteTransaction* trans,
                                             const Id& id,
                                             const Cryptographer* cryptographer,
                                             StatusController* status) {
  MutableEntry entry(trans, syncable::GET_BY_ID, id);
  // Must be good as the entry won't have been cleaned up.
  CHECK(entry.good());

  // This function can only resolve simple conflicts.  Simple conflicts have
  // both IS_UNSYNCED and IS_UNAPPLIED_UDPATE set.
  if (!entry.Get(syncable::IS_UNAPPLIED_UPDATE) ||
      !entry.Get(syncable::IS_UNSYNCED)) {
    // This is very unusual, but it can happen in tests.  We may be able to
    // assert NOTREACHED() here when those tests are updated.
    return;
  }

  if (entry.Get(syncable::IS_DEL) && entry.Get(syncable::SERVER_IS_DEL)) {
    // we've both deleted it, so lets just drop the need to commit/update this
    // entry.
    entry.Put(syncable::IS_UNSYNCED, false);
    entry.Put(syncable::IS_UNAPPLIED_UPDATE, false);
    // we've made changes, but they won't help syncing progress.
    // METRIC simple conflict resolved by merge.
    return;
  }

  // This logic determines "client wins" vs. "server wins" strategy picking.
  // By the time we get to this point, we rely on the following to be true:
  // a) We can decrypt both the local and server data (else we'd be in
  //    conflict encryption and not attempting to resolve).
  // b) All unsynced changes have been re-encrypted with the default key (
  //    occurs either in AttemptToUpdateEntry, SetEncryptionPassphrase,
  //    SetDecryptionPassphrase, or RefreshEncryption).
  // c) Base_server_specifics having a valid datatype means that we received
  //    an undecryptable update that only changed specifics, and since then have
  //    not received any further non-specifics-only or decryptable updates.
  // d) If the server_specifics match specifics, server_specifics are
  //    encrypted with the default key, and all other visible properties match,
  //    then we can safely ignore the local changes as redundant.
  // e) Otherwise if the base_server_specifics match the server_specifics, no
  //    functional change must have been made server-side (else
  //    base_server_specifics would have been cleared), and we can therefore
  //    safely ignore the server changes as redundant.
  // f) Otherwise, it's in general safer to ignore local changes, with the
  //    exception of deletion conflicts (choose to undelete) and conflicts
  //    where the non_unique_name or parent don't match.
  if (!entry.Get(syncable::SERVER_IS_DEL)) {
    // TODO(nick): The current logic is arbitrary; instead, it ought to be made
    // consistent with the ModelAssociator behavior for a datatype.  It would
    // be nice if we could route this back to ModelAssociator code to pick one
    // of three options: CLIENT, SERVER, or MERGE.  Some datatypes (autofill)
    // are easily mergeable.
    // See http://crbug.com/77339.
    bool name_matches = entry.Get(syncable::NON_UNIQUE_NAME) ==
                        entry.Get(syncable::SERVER_NON_UNIQUE_NAME);
    bool parent_matches = entry.Get(syncable::PARENT_ID) ==
                          entry.Get(syncable::SERVER_PARENT_ID);
    bool entry_deleted = entry.Get(syncable::IS_DEL);

    // This positional check is meant to be necessary but not sufficient. As a
    // result, it may be false even when the position hasn't changed, possibly
    // resulting in unnecessary commits, but if it's true the position has
    // definitely not changed. The check works by verifying that the prev id
    // as calculated from the server position (which will ignore any
    // unsynced/unapplied predecessors and be root for non-bookmark datatypes)
    // matches the client prev id. Because we traverse chains of conflicting
    // items in predecessor -> successor order, we don't need to also verify the
    // successor matches (If it's in conflict, we'll verify it next. If it's
    // not, then it should be taken into account already in the
    // ComputePrevIdFromServerPosition calculation). This works even when there
    // are chains of conflicting items.
    //
    // Example: Original sequence was abcde. Server changes to aCDbe, while
    // client changes to aDCbe (C and D are in conflict). Locally, D's prev id
    // is a, while C's prev id is D. On the other hand, the server prev id will
    // ignore unsynced/unapplied items, so D's server prev id will also be a,
    // just like C's. Because we traverse in client predecessor->successor
    // order, we evaluate D first. Since prev id and server id match, we
    // consider the position to have remained the same for D, and will unset
    // it's UNSYNCED/UNAPPLIED bits. When we evaluate C though, we'll see that
    // the prev id is D locally while the server's prev id is a. C will
    // therefore count as a positional conflict (and the local data will be
    // overwritten by the server data typically). The final result will be
    // aCDbe (the same as the server's view). Even though both C and D were
    // modified, only one counted as being in actual conflict and was resolved
    // with local/server wins.
    //
    // In general, when there are chains of positional conflicts, only the first
    // item in chain (based on the clients point of view) will have both its
    // server prev id and local prev id match. For all the rest the server prev
    // id will be the predecessor of the first item in the chain, and therefore
    // not match the local prev id.
    //
    // Similarly, chains of conflicts where the server and client info are the
    // same are supported due to the predecessor->successor ordering. In this
    // case, from the first item onward, we unset the UNSYNCED/UNAPPLIED bits as
    // we decide that nothing changed. The subsequent item's server prev id will
    // accurately match the local prev id because the predecessor is no longer
    // UNSYNCED/UNAPPLIED.
    // TODO(zea): simplify all this once we can directly compare server position
    // to client position.
    syncable::Id server_prev_id = entry.ComputePrevIdFromServerPosition(
        entry.Get(syncable::SERVER_PARENT_ID));
    bool needs_reinsertion = !parent_matches ||
         server_prev_id != entry.GetPredecessorId();
    DVLOG_IF(1, needs_reinsertion) << "Insertion needed, server prev id "
        << " is " << server_prev_id << ", local prev id is "
        << entry.GetPredecessorId();
    const sync_pb::EntitySpecifics& specifics =
        entry.Get(syncable::SPECIFICS);
    const sync_pb::EntitySpecifics& server_specifics =
        entry.Get(syncable::SERVER_SPECIFICS);
    const sync_pb::EntitySpecifics& base_server_specifics =
        entry.Get(syncable::BASE_SERVER_SPECIFICS);
    std::string decrypted_specifics, decrypted_server_specifics;
    bool specifics_match = false;
    bool server_encrypted_with_default_key = false;
    if (specifics.has_encrypted()) {
      DCHECK(cryptographer->CanDecryptUsingDefaultKey(specifics.encrypted()));
      decrypted_specifics = cryptographer->DecryptToString(
          specifics.encrypted());
    } else {
      decrypted_specifics = specifics.SerializeAsString();
    }
    if (server_specifics.has_encrypted()) {
      server_encrypted_with_default_key =
          cryptographer->CanDecryptUsingDefaultKey(
              server_specifics.encrypted());
      decrypted_server_specifics = cryptographer->DecryptToString(
          server_specifics.encrypted());
    } else {
      decrypted_server_specifics = server_specifics.SerializeAsString();
    }
    if (decrypted_server_specifics == decrypted_specifics &&
        server_encrypted_with_default_key == specifics.has_encrypted()) {
      specifics_match = true;
    }
    bool base_server_specifics_match = false;
    if (server_specifics.has_encrypted() &&
        IsRealDataType(GetModelTypeFromSpecifics(base_server_specifics))) {
      std::string decrypted_base_server_specifics;
      if (!base_server_specifics.has_encrypted()) {
        decrypted_base_server_specifics =
            base_server_specifics.SerializeAsString();
      } else {
        decrypted_base_server_specifics = cryptographer->DecryptToString(
            base_server_specifics.encrypted());
      }
      if (decrypted_server_specifics == decrypted_base_server_specifics)
          base_server_specifics_match = true;
    }

    if (!entry_deleted && name_matches && parent_matches && specifics_match &&
        !needs_reinsertion) {
      DVLOG(1) << "Resolving simple conflict, everything matches, ignoring "
               << "changes for: " << entry;
      conflict_util::IgnoreConflict(&entry);
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                                CHANGES_MATCH,
                                CONFLICT_RESOLUTION_SIZE);
    } else if (base_server_specifics_match) {
      DVLOG(1) << "Resolving simple conflict, ignoring server encryption "
               << " changes for: " << entry;
      status->increment_num_server_overwrites();
      conflict_util::OverwriteServerChanges(&entry);
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                                IGNORE_ENCRYPTION,
                                CONFLICT_RESOLUTION_SIZE);
    } else if (entry_deleted || !name_matches || !parent_matches) {
      // NOTE: The update application logic assumes that conflict resolution
      // will never result in changes to the local hierarchy.  The entry_deleted
      // and !parent_matches cases here are critical to maintaining that
      // assumption.
      conflict_util::OverwriteServerChanges(&entry);
      status->increment_num_server_overwrites();
      DVLOG(1) << "Resolving simple conflict, overwriting server changes "
               << "for: " << entry;
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                                OVERWRITE_SERVER,
                                CONFLICT_RESOLUTION_SIZE);
    } else {
      DVLOG(1) << "Resolving simple conflict, ignoring local changes for: "
               << entry;
      conflict_util::IgnoreLocalChanges(&entry);
      status->increment_num_local_overwrites();
      UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                                OVERWRITE_LOCAL,
                                CONFLICT_RESOLUTION_SIZE);
    }
    // Now that we've resolved the conflict, clear the prev server
    // specifics.
    entry.Put(syncable::BASE_SERVER_SPECIFICS, sync_pb::EntitySpecifics());
  } else {  // SERVER_IS_DEL is true
    if (entry.Get(syncable::IS_DIR)) {
      Directory::ChildHandles children;
      trans->directory()->GetChildHandlesById(trans,
                                              entry.Get(syncable::ID),
                                              &children);
      // If a server deleted folder has local contents it should be a hierarchy
      // conflict.  Hierarchy conflicts should not be processed by this
      // function.
      DCHECK(children.empty());
    }

    // The entry is deleted on the server but still exists locally.
    // We undelete it by overwriting the server's tombstone with the local
    // data.
    conflict_util::OverwriteServerChanges(&entry);
    status->increment_num_server_overwrites();
    DVLOG(1) << "Resolving simple conflict, undeleting server entry: "
             << entry;
    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                              UNDELETE,
                              CONFLICT_RESOLUTION_SIZE);
  }
}

void ConflictResolver::ResolveConflicts(
    syncable::WriteTransaction* trans,
    const Cryptographer* cryptographer,
    const std::set<syncable::Id>& simple_conflict_ids,
    sessions::StatusController* status) {
  // Iterate over simple conflict items.
  set<Id>::const_iterator conflicting_item_it;
  set<Id> processed_items;
  for (conflicting_item_it = simple_conflict_ids.begin();
       conflicting_item_it != simple_conflict_ids.end();
       ++conflicting_item_it) {
    Id id = *conflicting_item_it;
    if (processed_items.count(id) > 0)
      continue;

    // We don't resolve conflicts for control types here.
    Entry conflicting_node(trans, syncable::GET_BY_ID, id);
    CHECK(conflicting_node.good());
    if (IsControlType(
        GetModelTypeFromSpecifics(conflicting_node.Get(syncable::SPECIFICS)))) {
      continue;
    }

    // We have a simple conflict. In order check if positions have changed,
    // we need to process conflicting predecessors before successors. Traverse
    // backwards through all continuous conflicting predecessors, building a
    // stack of items to resolve in predecessor->successor order, then process
    // each item individually.
    list<Id> predecessors;
    Id prev_id = id;
    do {
      predecessors.push_back(prev_id);
      Entry entry(trans, syncable::GET_BY_ID, prev_id);
      // Any entry in conflict must be valid.
      CHECK(entry.good());
      Id new_prev_id = entry.GetPredecessorId();
      if (new_prev_id == prev_id)
        break;
      prev_id = new_prev_id;
    } while (processed_items.count(prev_id) == 0 &&
             simple_conflict_ids.count(prev_id) > 0);  // Excludes root.
    while (!predecessors.empty()) {
      id = predecessors.back();
      predecessors.pop_back();
      ProcessSimpleConflict(trans, id, cryptographer, status);
      processed_items.insert(id);
    }
  }
  return;
}

}  // namespace syncer
