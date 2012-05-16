// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/syncer_util.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/metrics/histogram.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/nigori_util.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/engine/syncer_types.h"
#include "sync/engine/syncproto.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/password_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/model_type.h"
#include "sync/syncable/syncable.h"
#include "sync/syncable/syncable_changes_version.h"
#include "sync/util/cryptographer.h"
#include "sync/util/time.h"

using syncable::BASE_VERSION;
using syncable::Blob;
using syncable::CHANGES_VERSION;
using syncable::CREATE;
using syncable::CREATE_NEW_UPDATE_ITEM;
using syncable::CTIME;
using syncable::Directory;
using syncable::Entry;
using syncable::GetModelTypeFromSpecifics;
using syncable::GET_BY_HANDLE;
using syncable::GET_BY_ID;
using syncable::ID;
using syncable::IS_DEL;
using syncable::IS_DIR;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::Id;
using syncable::IsRealDataType;
using syncable::META_HANDLE;
using syncable::MTIME;
using syncable::MutableEntry;
using syncable::NEXT_ID;
using syncable::NON_UNIQUE_NAME;
using syncable::BASE_SERVER_SPECIFICS;
using syncable::PARENT_ID;
using syncable::PREV_ID;
using syncable::ReadTransaction;
using syncable::SERVER_CTIME;
using syncable::SERVER_IS_DEL;
using syncable::SERVER_IS_DIR;
using syncable::SERVER_MTIME;
using syncable::SERVER_NON_UNIQUE_NAME;
using syncable::SERVER_PARENT_ID;
using syncable::SERVER_POSITION_IN_PARENT;
using syncable::SERVER_SPECIFICS;
using syncable::SERVER_VERSION;
using syncable::UNIQUE_CLIENT_TAG;
using syncable::UNIQUE_SERVER_TAG;
using syncable::SPECIFICS;
using syncable::SYNCER;
using syncable::WriteTransaction;

namespace browser_sync {

// Returns the number of unsynced entries.
// static
int SyncerUtil::GetUnsyncedEntries(syncable::BaseTransaction* trans,
                                   std::vector<int64> *handles) {
  trans->directory()->GetUnsyncedMetaHandles(trans, handles);
  DVLOG_IF(1, !handles->empty()) << "Have " << handles->size()
                                 << " unsynced items.";
  return handles->size();
}

// static
syncable::Id SyncerUtil::FindLocalIdToUpdate(
    syncable::BaseTransaction* trans,
    const SyncEntity& update) {
  // Expected entry points of this function:
  // SyncEntity has NOT been applied to SERVER fields.
  // SyncEntity has NOT been applied to LOCAL fields.
  // DB has not yet been modified, no entries created for this update.

  const std::string& client_id = trans->directory()->cache_guid();

  if (update.has_client_defined_unique_tag() &&
      !update.client_defined_unique_tag().empty()) {
    // When a server sends down a client tag, the following cases can occur:
    // 1) Client has entry for tag already, ID is server style, matches
    // 2) Client has entry for tag already, ID is server, doesn't match.
    // 3) Client has entry for tag already, ID is local, (never matches)
    // 4) Client has no entry for tag

    // Case 1, we don't have to do anything since the update will
    // work just fine. Update will end up in the proper entry, via ID lookup.
    // Case 2 - Happens very rarely due to lax enforcement of client tags
    // on the server, if two clients commit the same tag at the same time.
    // When this happens, we pick the lexically-least ID and ignore all other
    // items.
    // Case 3 - We need to replace the local ID with the server ID so that
    // this update gets targeted at the correct local entry; we expect conflict
    // resolution to occur.
    // Case 4 - Perfect. Same as case 1.

    syncable::Entry local_entry(trans, syncable::GET_BY_CLIENT_TAG,
                                update.client_defined_unique_tag());

    // The SyncAPI equivalent of this function will return !good if IS_DEL.
    // The syncable version will return good even if IS_DEL.
    // TODO(chron): Unit test the case with IS_DEL and make sure.
    if (local_entry.good()) {
      if (local_entry.Get(ID).ServerKnows()) {
        if (local_entry.Get(ID) != update.id()) {
          // Case 2.
          LOG(WARNING) << "Duplicated client tag.";
          if (local_entry.Get(ID) < update.id()) {
            // Signal an error; drop this update on the floor.  Note that
            // we don't server delete the item, because we don't allow it to
            // exist locally at all.  So the item will remain orphaned on
            // the server, and we won't pay attention to it.
            return syncable::GetNullId();
          }
        }
        // Target this change to the existing local entry; later,
        // we'll change the ID of the local entry to update.id()
        // if needed.
        return local_entry.Get(ID);
      } else {
        // Case 3: We have a local entry with the same client tag.
        // We should change the ID of the local entry to the server entry.
        // This will result in an server ID with base version == 0, but that's
        // a legal state for an item with a client tag.  By changing the ID,
        // update will now be applied to local_entry.
        DCHECK(0 == local_entry.Get(BASE_VERSION) ||
               CHANGES_VERSION == local_entry.Get(BASE_VERSION));
        return local_entry.Get(ID);
      }
    }
  } else if (update.has_originator_cache_guid() &&
      update.originator_cache_guid() == client_id) {
    // If a commit succeeds, but the response does not come back fast enough
    // then the syncer might assume that it was never committed.
    // The server will track the client that sent up the original commit and
    // return this in a get updates response. When this matches a local
    // uncommitted item, we must mutate our local item and version to pick up
    // the committed version of the same item whose commit response was lost.
    // There is however still a race condition if the server has not
    // completed the commit by the time the syncer tries to get updates
    // again. To mitigate this, we need to have the server time out in
    // a reasonable span, our commit batches have to be small enough
    // to process within our HTTP response "assumed alive" time.

    // We need to check if we have an entry that didn't get its server
    // id updated correctly. The server sends down a client ID
    // and a local (negative) id. If we have a entry by that
    // description, we should update the ID and version to the
    // server side ones to avoid multiple copies of the same thing.

    syncable::Id client_item_id = syncable::Id::CreateFromClientString(
        update.originator_client_item_id());
    DCHECK(!client_item_id.ServerKnows());
    syncable::Entry local_entry(trans, GET_BY_ID, client_item_id);

    // If it exists, then our local client lost a commit response.  Use
    // the local entry.
    if (local_entry.good() && !local_entry.Get(IS_DEL)) {
      int64 old_version = local_entry.Get(BASE_VERSION);
      int64 new_version = update.version();
      DCHECK_LE(old_version, 0);
      DCHECK_GT(new_version, 0);
      // Otherwise setting the base version could cause a consistency failure.
      // An entry should never be version 0 and SYNCED.
      DCHECK(local_entry.Get(IS_UNSYNCED));

      // Just a quick sanity check.
      DCHECK(!local_entry.Get(ID).ServerKnows());

      DVLOG(1) << "Reuniting lost commit response IDs. server id: "
               << update.id() << " local id: " << local_entry.Get(ID)
               << " new version: " << new_version;

      return local_entry.Get(ID);
    }
  }
  // Fallback: target an entry having the server ID, creating one if needed.
  return update.id();
}

// static
UpdateAttemptResponse SyncerUtil::AttemptToUpdateEntry(
    syncable::WriteTransaction* const trans,
    syncable::MutableEntry* const entry,
    ConflictResolver* resolver,
    Cryptographer* cryptographer) {
  CHECK(entry->good());
  if (!entry->Get(IS_UNAPPLIED_UPDATE))
    return SUCCESS;  // No work to do.
  syncable::Id id = entry->Get(ID);
  const sync_pb::EntitySpecifics& specifics = entry->Get(SERVER_SPECIFICS);

  // We intercept updates to the Nigori node, update the Cryptographer and
  // encrypt any unsynced changes here because there is no Nigori
  // ChangeProcessor. We never put the nigori node in a state of
  // conflict_encryption.
  //
  // We always update the cryptographer with the server's nigori node,
  // even if we have a locally modified nigori node (we manually merge nigori
  // data in the conflict resolver in that case). This handles the case where
  // two clients both set a different passphrase. The second client to attempt
  // to commit will go into a state of having pending keys, unioned the set of
  // encrypted types, and eventually re-encrypt everything with the passphrase
  // of the first client and commit the set of merged encryption keys. Until the
  // second client provides the pending passphrase, the cryptographer will
  // preserve the encryption keys based on the local passphrase, while the
  // nigori node will preserve the server encryption keys.
  //
  // If non-encryption changes are made to the nigori node, they will be
  // lost as part of conflict resolution. This is intended, as we place a higher
  // priority on preserving the server's passphrase change to preserving local
  // non-encryption changes. Next time the non-encryption changes are made to
  // the nigori node (e.g. on restart), they will commit without issue.
  if (specifics.has_nigori()) {
    const sync_pb::NigoriSpecifics& nigori = specifics.nigori();
    cryptographer->Update(nigori);

    // Make sure any unsynced changes are properly encrypted as necessary.
    // We only perform this if the cryptographer is ready. If not, these are
    // re-encrypted at SetDecryptionPassphrase time (via ReEncryptEverything).
    // This logic covers the case where the nigori update marked new datatypes
    // for encryption, but didn't change the passphrase.
    if (cryptographer->is_ready()) {
      // Note that we don't bother to encrypt any data for which IS_UNSYNCED
      // == false here. The machine that turned on encryption should know about
      // and re-encrypt all synced data. It's possible it could get interrupted
      // during this process, but we currently reencrypt everything at startup
      // as well, so as soon as a client is restarted with this datatype marked
      // for encryption, all the data should be updated as necessary.

      // If this fails, something is wrong with the cryptographer, but there's
      // nothing we can do about it here.
      DVLOG(1) << "Received new nigori, encrypting unsynced changes.";
      syncable::ProcessUnsyncedChangesForEncryption(trans, cryptographer);
    }
  }

  // Only apply updates that we can decrypt. If we can't decrypt the update, it
  // is likely because the passphrase has not arrived yet. Because the
  // passphrase may not arrive within this GetUpdates, we can't just return
  // conflict, else we try to perform normal conflict resolution prematurely or
  // the syncer may get stuck. As such, we return CONFLICT_ENCRYPTION, which is
  // treated as an unresolvable conflict. See the description in syncer_types.h.
  // This prevents any unsynced changes from commiting and postpones conflict
  // resolution until all data can be decrypted.
  if (specifics.has_encrypted() &&
      !cryptographer->CanDecrypt(specifics.encrypted())) {
    // We can't decrypt this node yet.
    DVLOG(1) << "Received an undecryptable "
             << syncable::ModelTypeToString(entry->GetServerModelType())
             << " update, returning encryption_conflict.";
    return CONFLICT_ENCRYPTION;
  } else if (specifics.has_password() &&
             entry->Get(UNIQUE_SERVER_TAG).empty()) {
    // Passwords use their own legacy encryption scheme.
    const sync_pb::PasswordSpecifics& password = specifics.password();
    if (!cryptographer->CanDecrypt(password.encrypted())) {
      DVLOG(1) << "Received an undecryptable password update, returning "
               << "encryption_conflict.";
      return CONFLICT_ENCRYPTION;
    }
  }

  if (!entry->Get(SERVER_IS_DEL)) {
    syncable::Id new_parent = entry->Get(SERVER_PARENT_ID);
    Entry parent(trans, GET_BY_ID,  new_parent);
    // A note on non-directory parents:
    // We catch most unfixable tree invariant errors at update receipt time,
    // however we deal with this case here because we may receive the child
    // first then the illegal parent. Instead of dealing with it twice in
    // different ways we deal with it once here to reduce the amount of code and
    // potential errors.
    if (!parent.good() || parent.Get(IS_DEL) || !parent.Get(IS_DIR)) {
      return CONFLICT_HIERARCHY;
    }
    if (entry->Get(PARENT_ID) != new_parent) {
      if (!entry->Get(IS_DEL) && !IsLegalNewParent(trans, id, new_parent)) {
        DVLOG(1) << "Not updating item " << id
                 << ", illegal new parent (would cause loop).";
        return CONFLICT_HIERARCHY;
      }
    }
  } else if (entry->Get(IS_DIR)) {
    Directory::ChildHandles handles;
    trans->directory()->GetChildHandlesById(trans, id, &handles);
    if (!handles.empty()) {
      // If we have still-existing children, then we need to deal with
      // them before we can process this change.
      DVLOG(1) << "Not deleting directory; it's not empty " << *entry;
      return CONFLICT_HIERARCHY;
    }
  }

  if (entry->Get(IS_UNSYNCED)) {
    DVLOG(1) << "Skipping update, returning conflict for: " << id
             << " ; it's unsynced.";
    return CONFLICT_SIMPLE;
  }

  if (specifics.has_encrypted()) {
    DVLOG(2) << "Received a decryptable "
             << syncable::ModelTypeToString(entry->GetServerModelType())
             << " update, applying normally.";
  } else {
    DVLOG(2) << "Received an unencrypted "
             << syncable::ModelTypeToString(entry->GetServerModelType())
             << " update, applying normally.";
  }

  SyncerUtil::UpdateLocalDataFromServerData(trans, entry);

  return SUCCESS;
}

namespace {
// Helper to synthesize a new-style sync_pb::EntitySpecifics for use locally,
// when the server speaks only the old sync_pb::SyncEntity_BookmarkData-based
// protocol.
void UpdateBookmarkSpecifics(const std::string& singleton_tag,
                             const std::string& url,
                             const std::string& favicon_bytes,
                             MutableEntry* local_entry) {
  // In the new-style protocol, the server no longer sends bookmark info for
  // the "google_chrome" folder.  Mimic that here.
  if (singleton_tag == "google_chrome")
    return;
  sync_pb::EntitySpecifics pb;
  sync_pb::BookmarkSpecifics* bookmark = pb.mutable_bookmark();
  if (!url.empty())
    bookmark->set_url(url);
  if (!favicon_bytes.empty())
    bookmark->set_favicon(favicon_bytes);
  local_entry->Put(SERVER_SPECIFICS, pb);
}

}  // namespace

// Pass in name and checksum because of UTF8 conversion.
// static
void SyncerUtil::UpdateServerFieldsFromUpdate(
    MutableEntry* target,
    const SyncEntity& update,
    const std::string& name) {
  if (update.deleted()) {
    if (target->Get(SERVER_IS_DEL)) {
      // If we already think the item is server-deleted, we're done.
      // Skipping these cases prevents our committed deletions from coming
      // back and overriding subsequent undeletions.  For non-deleted items,
      // the version number check has a similar effect.
      return;
    }
    // The server returns very lightweight replies for deletions, so we don't
    // clobber a bunch of fields on delete.
    target->Put(SERVER_IS_DEL, true);
    if (!target->Get(UNIQUE_CLIENT_TAG).empty()) {
      // Items identified by the client unique tag are undeletable; when
      // they're deleted, they go back to version 0.
      target->Put(SERVER_VERSION, 0);
    } else {
      // Otherwise, fake a server version by bumping the local number.
      target->Put(SERVER_VERSION,
          std::max(target->Get(SERVER_VERSION),
                   target->Get(BASE_VERSION)) + 1);
    }
    target->Put(IS_UNAPPLIED_UPDATE, true);
    return;
  }

  DCHECK(target->Get(ID) == update.id())
      << "ID Changing not supported here";
  target->Put(SERVER_PARENT_ID, update.parent_id());
  target->Put(SERVER_NON_UNIQUE_NAME, name);
  target->Put(SERVER_VERSION, update.version());
  target->Put(SERVER_CTIME, ProtoTimeToTime(update.ctime()));
  target->Put(SERVER_MTIME, ProtoTimeToTime(update.mtime()));
  target->Put(SERVER_IS_DIR, update.IsFolder());
  if (update.has_server_defined_unique_tag()) {
    const std::string& tag = update.server_defined_unique_tag();
    target->Put(UNIQUE_SERVER_TAG, tag);
  }
  if (update.has_client_defined_unique_tag()) {
    const std::string& tag = update.client_defined_unique_tag();
    target->Put(UNIQUE_CLIENT_TAG, tag);
  }
  // Store the datatype-specific part as a protobuf.
  if (update.has_specifics()) {
    DCHECK(update.GetModelType() != syncable::UNSPECIFIED)
        << "Storing unrecognized datatype in sync database.";
    target->Put(SERVER_SPECIFICS, update.specifics());
  } else if (update.has_bookmarkdata()) {
    // Legacy protocol response for bookmark data.
    const SyncEntity::BookmarkData& bookmark = update.bookmarkdata();
    UpdateBookmarkSpecifics(update.server_defined_unique_tag(),
                            bookmark.bookmark_url(),
                            bookmark.bookmark_favicon(),
                            target);
  }
  if (update.has_position_in_parent())
    target->Put(SERVER_POSITION_IN_PARENT, update.position_in_parent());

  target->Put(SERVER_IS_DEL, update.deleted());
  // We only mark the entry as unapplied if its version is greater than the
  // local data. If we're processing the update that corresponds to one of our
  // commit we don't apply it as time differences may occur.
  if (update.version() > target->Get(BASE_VERSION)) {
    target->Put(IS_UNAPPLIED_UPDATE, true);
  }
}

// Creates a new Entry iff no Entry exists with the given id.
// static
void SyncerUtil::CreateNewEntry(syncable::WriteTransaction *trans,
                                const syncable::Id& id) {
  syncable::MutableEntry entry(trans, GET_BY_ID, id);
  if (!entry.good()) {
    syncable::MutableEntry new_entry(trans, syncable::CREATE_NEW_UPDATE_ITEM,
                                     id);
  }
}

// static
void SyncerUtil::SplitServerInformationIntoNewEntry(
    syncable::WriteTransaction* trans,
    syncable::MutableEntry* entry) {
  syncable::Id id = entry->Get(ID);
  ChangeEntryIDAndUpdateChildren(trans, entry, trans->directory()->NextId());
  entry->Put(BASE_VERSION, 0);

  MutableEntry new_entry(trans, CREATE_NEW_UPDATE_ITEM, id);
  CopyServerFields(entry, &new_entry);
  ClearServerData(entry);

  DVLOG(1) << "Splitting server information, local entry: " << *entry
           << " server entry: " << new_entry;
}

// This function is called on an entry when we can update the user-facing data
// from the server data.
// static
void SyncerUtil::UpdateLocalDataFromServerData(
    syncable::WriteTransaction* trans,
    syncable::MutableEntry* entry) {
  DCHECK(!entry->Get(IS_UNSYNCED));
  DCHECK(entry->Get(IS_UNAPPLIED_UPDATE));

  DVLOG(2) << "Updating entry : " << *entry;
  // Start by setting the properties that determine the model_type.
  entry->Put(SPECIFICS, entry->Get(SERVER_SPECIFICS));
  // Clear the previous server specifics now that we're applying successfully.
  entry->Put(BASE_SERVER_SPECIFICS, sync_pb::EntitySpecifics());
  entry->Put(IS_DIR, entry->Get(SERVER_IS_DIR));
  // This strange dance around the IS_DEL flag avoids problems when setting
  // the name.
  // TODO(chron): Is this still an issue? Unit test this codepath.
  if (entry->Get(SERVER_IS_DEL)) {
    entry->Put(IS_DEL, true);
  } else {
    entry->Put(NON_UNIQUE_NAME, entry->Get(SERVER_NON_UNIQUE_NAME));
    entry->Put(PARENT_ID, entry->Get(SERVER_PARENT_ID));
    CHECK(entry->Put(IS_DEL, false));
    Id new_predecessor =
        entry->ComputePrevIdFromServerPosition(entry->Get(SERVER_PARENT_ID));
    CHECK(entry->PutPredecessor(new_predecessor))
        << " Illegal predecessor after converting from server position.";
  }

  entry->Put(CTIME, entry->Get(SERVER_CTIME));
  entry->Put(MTIME, entry->Get(SERVER_MTIME));
  entry->Put(BASE_VERSION, entry->Get(SERVER_VERSION));
  entry->Put(IS_DEL, entry->Get(SERVER_IS_DEL));
  entry->Put(IS_UNAPPLIED_UPDATE, false);
}

// static
VerifyCommitResult SyncerUtil::ValidateCommitEntry(
    syncable::Entry* entry) {
  syncable::Id id = entry->Get(ID);
  if (id == entry->Get(PARENT_ID)) {
    CHECK(id.IsRoot()) << "Non-root item is self parenting." << *entry;
    // If the root becomes unsynced it can cause us problems.
    LOG(ERROR) << "Root item became unsynced " << *entry;
    return VERIFY_UNSYNCABLE;
  }
  if (entry->IsRoot()) {
    LOG(ERROR) << "Permanent item became unsynced " << *entry;
    return VERIFY_UNSYNCABLE;
  }
  if (entry->Get(IS_DEL) && !entry->Get(ID).ServerKnows()) {
    // Drop deleted uncommitted entries.
    return VERIFY_UNSYNCABLE;
  }
  return VERIFY_OK;
}

// static
bool SyncerUtil::AddItemThenPredecessors(
    syncable::BaseTransaction* trans,
    syncable::Entry* item,
    syncable::IndexedBitField inclusion_filter,
    syncable::MetahandleSet* inserted_items,
    std::vector<syncable::Id>* commit_ids) {

  if (!inserted_items->insert(item->Get(META_HANDLE)).second)
    return false;
  commit_ids->push_back(item->Get(ID));
  if (item->Get(IS_DEL))
    return true;  // Deleted items have no predecessors.

  Id prev_id = item->Get(PREV_ID);
  while (!prev_id.IsRoot()) {
    Entry prev(trans, GET_BY_ID, prev_id);
    CHECK(prev.good()) << "Bad id when walking predecessors.";
    if (!prev.Get(inclusion_filter))
      break;
    if (!inserted_items->insert(prev.Get(META_HANDLE)).second)
      break;
    commit_ids->push_back(prev_id);
    prev_id = prev.Get(PREV_ID);
  }
  return true;
}

// static
void SyncerUtil::AddPredecessorsThenItem(
    syncable::BaseTransaction* trans,
    syncable::Entry* item,
    syncable::IndexedBitField inclusion_filter,
    syncable::MetahandleSet* inserted_items,
    std::vector<syncable::Id>* commit_ids) {
  size_t initial_size = commit_ids->size();
  if (!AddItemThenPredecessors(trans, item, inclusion_filter, inserted_items,
                               commit_ids))
    return;
  // Reverse what we added to get the correct order.
  std::reverse(commit_ids->begin() + initial_size, commit_ids->end());
}

// static
void SyncerUtil::MarkDeletedChildrenSynced(
    syncable::Directory* dir,
    std::set<syncable::Id>* deleted_folders) {
  // There's two options here.
  // 1. Scan deleted unsynced entries looking up their pre-delete tree for any
  //    of the deleted folders.
  // 2. Take each folder and do a tree walk of all entries underneath it.
  // #2 has a lower big O cost, but writing code to limit the time spent inside
  // the transaction during each step is simpler with 1. Changing this decision
  // may be sensible if this code shows up in profiling.
  if (deleted_folders->empty())
    return;
  Directory::UnsyncedMetaHandles handles;
  {
    ReadTransaction trans(FROM_HERE, dir);
    dir->GetUnsyncedMetaHandles(&trans, &handles);
  }
  if (handles.empty())
    return;
  Directory::UnsyncedMetaHandles::iterator it;
  for (it = handles.begin() ; it != handles.end() ; ++it) {
    // Single transaction / entry we deal with.
    WriteTransaction trans(FROM_HERE, SYNCER, dir);
    MutableEntry entry(&trans, GET_BY_HANDLE, *it);
    if (!entry.Get(IS_UNSYNCED) || !entry.Get(IS_DEL))
      continue;
    syncable::Id id = entry.Get(PARENT_ID);
    while (id != trans.root_id()) {
      if (deleted_folders->find(id) != deleted_folders->end()) {
        // We've synced the deletion of this deleted entries parent.
        entry.Put(IS_UNSYNCED, false);
        break;
      }
      Entry parent(&trans, GET_BY_ID, id);
      if (!parent.good() || !parent.Get(IS_DEL))
        break;
      id = parent.Get(PARENT_ID);
    }
  }
}

// static
VerifyResult SyncerUtil::VerifyNewEntry(
    const SyncEntity& update,
    syncable::Entry* target,
    const bool deleted) {
  if (target->good()) {
    // Not a new update.
    return VERIFY_UNDECIDED;
  }
  if (deleted) {
    // Deletion of an item we've never seen can be ignored.
    return VERIFY_SKIP;
  }

  return VERIFY_SUCCESS;
}

// Assumes we have an existing entry; check here for updates that break
// consistency rules.
// static
VerifyResult SyncerUtil::VerifyUpdateConsistency(
    syncable::WriteTransaction* trans,
    const SyncEntity& update,
    syncable::MutableEntry* target,
    const bool deleted,
    const bool is_directory,
    syncable::ModelType model_type) {

  CHECK(target->good());

  // If the update is a delete, we don't really need to worry at this stage.
  if (deleted)
    return VERIFY_SUCCESS;

  if (model_type == syncable::UNSPECIFIED) {
    // This update is to an item of a datatype we don't recognize. The server
    // shouldn't have sent it to us.  Throw it on the ground.
    return VERIFY_SKIP;
  }

  if (target->Get(SERVER_VERSION) > 0) {
    // Then we've had an update for this entry before.
    if (is_directory != target->Get(SERVER_IS_DIR) ||
        model_type != target->GetServerModelType()) {
      if (target->Get(IS_DEL)) {  // If we've deleted the item, we don't care.
        return VERIFY_SKIP;
      } else {
        LOG(ERROR) << "Server update doesn't agree with previous updates. ";
        LOG(ERROR) << " Entry: " << *target;
        LOG(ERROR) << " Update: "
                   << SyncerProtoUtil::SyncEntityDebugString(update);
        return VERIFY_FAIL;
      }
    }

    if (!deleted && (target->Get(ID) == update.id()) &&
        (target->Get(SERVER_IS_DEL) ||
         (!target->Get(IS_UNSYNCED) && target->Get(IS_DEL) &&
          target->Get(BASE_VERSION) > 0))) {
      // An undelete. The latter case in the above condition is for
      // when the server does not give us an update following the
      // commit of a delete, before undeleting.
      // Undeletion is common for items that reuse the client-unique tag.
      VerifyResult result =
          SyncerUtil::VerifyUndelete(trans, update, target);
      if (VERIFY_UNDECIDED != result)
        return result;
    }
  }
  if (target->Get(BASE_VERSION) > 0) {
    // We've committed this update in the past.
    if (is_directory != target->Get(IS_DIR) ||
        model_type != target->GetModelType()) {
      LOG(ERROR) << "Server update doesn't agree with committed item. ";
      LOG(ERROR) << " Entry: " << *target;
      LOG(ERROR) << " Update: "
                 << SyncerProtoUtil::SyncEntityDebugString(update);
      return VERIFY_FAIL;
    }
    if (target->Get(ID) == update.id()) {
      if (target->Get(SERVER_VERSION) > update.version()) {
        LOG(WARNING) << "We've already seen a more recent version.";
        LOG(WARNING) << " Entry: " << *target;
        LOG(WARNING) << " Update: "
                     << SyncerProtoUtil::SyncEntityDebugString(update);
        return VERIFY_SKIP;
      }
    }
  }
  return VERIFY_SUCCESS;
}

// Assumes we have an existing entry; verify an update that seems to be
// expressing an 'undelete'
// static
VerifyResult SyncerUtil::VerifyUndelete(syncable::WriteTransaction* trans,
                                        const SyncEntity& update,
                                        syncable::MutableEntry* target) {
  // TODO(nick): We hit this path for items deleted items that the server
  // tells us to re-create; only deleted items with positive base versions
  // will hit this path.  However, it's not clear how such an undeletion
  // would actually succeed on the server; in the protocol, a base
  // version of 0 is required to undelete an object.  This codepath
  // should be deprecated in favor of client-tag style undeletion
  // (where items go to version 0 when they're deleted), or else
  // removed entirely (if this type of undeletion is indeed impossible).
  CHECK(target->good());
  DVLOG(1) << "Server update is attempting undelete. " << *target
           << "Update:" << SyncerProtoUtil::SyncEntityDebugString(update);
  // Move the old one aside and start over.  It's too tricky to get the old one
  // back into a state that would pass CheckTreeInvariants().
  if (target->Get(IS_DEL)) {
    DCHECK(target->Get(UNIQUE_CLIENT_TAG).empty())
        << "Doing move-aside undeletion on client-tagged item.";
    target->Put(ID, trans->directory()->NextId());
    target->Put(UNIQUE_CLIENT_TAG, "");
    target->Put(BASE_VERSION, CHANGES_VERSION);
    target->Put(SERVER_VERSION, 0);
    return VERIFY_SUCCESS;
  }
  if (update.version() < target->Get(SERVER_VERSION)) {
    LOG(WARNING) << "Update older than current server version for "
                 << *target << " Update:"
                 << SyncerProtoUtil::SyncEntityDebugString(update);
    return VERIFY_SUCCESS;  // Expected in new sync protocol.
  }
  return VERIFY_UNDECIDED;
}

}  // namespace browser_sync
