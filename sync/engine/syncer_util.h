// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions manipulating syncable::Entries, intended for use by the
// syncer.

#ifndef SYNC_ENGINE_SYNCER_UTIL_H_
#define SYNC_ENGINE_SYNCER_UTIL_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "sync/engine/syncer.h"
#include "sync/engine/syncer_types.h"
#include "sync/syncable/entry_kernel.h"
#include "sync/syncable/metahandle_set.h"
#include "sync/syncable/syncable_id.h"

namespace syncer {

class Cryptographer;
class SyncEntity;

// If the server sent down a client-tagged entry, or an entry whose
// commit response was lost, it is necessary to update a local entry
// with an ID that doesn't match the ID of the update.  Here, we
// find the ID of such an entry, if it exists.  This function may
// determine that |server_entry| should be dropped; if so, it returns
// the null ID -- callers must handle this case.  When update application
// should proceed normally with a new local entry, this function will
// return server_entry.id(); the caller must create an entry with that
// ID.  This function does not alter the database.
syncable::Id FindLocalIdToUpdate(
    syncable::BaseTransaction* trans,
    const SyncEntity& server_entry);

UpdateAttemptResponse AttemptToUpdateEntry(
    syncable::WriteTransaction* const trans,
    syncable::MutableEntry* const entry,
    ConflictResolver* resolver,
    Cryptographer* cryptographer);

// Pass in name to avoid redundant UTF8 conversion.
void UpdateServerFieldsFromUpdate(
    syncable::MutableEntry* local_entry,
    const SyncEntity& server_entry,
    const std::string& name);

// Creates a new Entry iff no Entry exists with the given id.
void CreateNewEntry(syncable::WriteTransaction *trans,
                    const syncable::Id& id);

void SplitServerInformationIntoNewEntry(
    syncable::WriteTransaction* trans,
    syncable::MutableEntry* entry);

// This function is called on an entry when we can update the user-facing data
// from the server data.
void UpdateLocalDataFromServerData(syncable::WriteTransaction* trans,
                                   syncable::MutableEntry* entry);

VerifyCommitResult ValidateCommitEntry(syncable::Entry* entry);

VerifyResult VerifyNewEntry(const SyncEntity& update,
                            syncable::Entry* target,
                            const bool deleted);

// Assumes we have an existing entry; check here for updates that break
// consistency rules.
VerifyResult VerifyUpdateConsistency(syncable::WriteTransaction* trans,
                                     const SyncEntity& update,
                                     syncable::MutableEntry* target,
                                     const bool deleted,
                                     const bool is_directory,
                                     syncable::ModelType model_type);

// Assumes we have an existing entry; verify an update that seems to be
// expressing an 'undelete'
VerifyResult VerifyUndelete(syncable::WriteTransaction* trans,
                            const SyncEntity& update,
                            syncable::MutableEntry* target);

// Append |item|, followed by a chain of its predecessors selected by
// |inclusion_filter|, to the |commit_ids| vector and tag them as included by
// storing in the set |inserted_items|.  |inclusion_filter| (typically one of
// IS_UNAPPLIED_UPDATE or IS_UNSYNCED) selects which type of predecessors to
// include.  Returns true if |item| was added, and false if it was already in
// the list.
//
// Use AddPredecessorsThenItem instead of this method if you want the
// item to be the last, rather than first, item appended.
bool AddItemThenPredecessors(
    syncable::BaseTransaction* trans,
    syncable::Entry* item,
    syncable::IndexedBitField inclusion_filter,
    syncable::MetahandleSet* inserted_items,
    std::vector<syncable::Id>* commit_ids);

// Exactly like AddItemThenPredecessors, except items are appended in the
// reverse (and generally more useful) order: a chain of predecessors from
// far to near, and finally the item.
void AddPredecessorsThenItem(
    syncable::BaseTransaction* trans,
    syncable::Entry* item,
    syncable::IndexedBitField inclusion_filter,
    syncable::MetahandleSet* inserted_items,
    std::vector<syncable::Id>* commit_ids);

void MarkDeletedChildrenSynced(
    syncable::Directory* dir,
    std::set<syncable::Id>* deleted_folders);

}  // namespace syncer

#endif  // SYNC_ENGINE_SYNCER_UTIL_H_
