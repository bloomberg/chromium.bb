// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/process_updates_command.h"

#include <vector>

#include "base/basictypes.h"
#include "base/location.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/engine/syncer_util.h"
#include "sync/engine/syncproto.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/syncable.h"
#include "sync/util/cryptographer.h"

using std::vector;

namespace browser_sync {

using sessions::SyncSession;
using sessions::StatusController;
using sessions::UpdateProgress;

ProcessUpdatesCommand::ProcessUpdatesCommand() {}
ProcessUpdatesCommand::~ProcessUpdatesCommand() {}

std::set<ModelSafeGroup> ProcessUpdatesCommand::GetGroupsToChange(
    const sessions::SyncSession& session) const {
  return session.GetEnabledGroupsWithVerifiedUpdates();
}

SyncerError ProcessUpdatesCommand::ModelChangingExecuteImpl(
    SyncSession* session) {
  syncable::Directory* dir = session->context()->directory();

  const sessions::UpdateProgress* progress =
      session->status_controller().update_progress();
  if (!progress)
    return SYNCER_OK;  // Nothing to do.

  syncable::WriteTransaction trans(FROM_HERE, syncable::SYNCER, dir);
  vector<sessions::VerifiedUpdate>::const_iterator it;
  for (it = progress->VerifiedUpdatesBegin();
       it != progress->VerifiedUpdatesEnd();
       ++it) {
    const sync_pb::SyncEntity& update = it->second;

    if (it->first != VERIFY_SUCCESS && it->first != VERIFY_UNDELETE)
      continue;
    switch (ProcessUpdate(update,
                          dir->GetCryptographer(&trans),
                          &trans)) {
      case SUCCESS_PROCESSED:
      case SUCCESS_STORED:
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  StatusController* status = session->mutable_status_controller();
  status->mutable_update_progress()->ClearVerifiedUpdates();
  return SYNCER_OK;
}

namespace {
// Returns true if the entry is still ok to process.
bool ReverifyEntry(syncable::WriteTransaction* trans, const SyncEntity& entry,
                   syncable::MutableEntry* same_id) {

  const bool deleted = entry.has_deleted() && entry.deleted();
  const bool is_directory = entry.IsFolder();
  const syncable::ModelType model_type = entry.GetModelType();

  return VERIFY_SUCCESS == SyncerUtil::VerifyUpdateConsistency(trans,
                                                               entry,
                                                               same_id,
                                                               deleted,
                                                               is_directory,
                                                               model_type);
}
}  // namespace

// Process a single update. Will avoid touching global state.
ServerUpdateProcessingResult ProcessUpdatesCommand::ProcessUpdate(
    const sync_pb::SyncEntity& proto_update,
    const Cryptographer* cryptographer,
    syncable::WriteTransaction* const trans) {

  const SyncEntity& update = *static_cast<const SyncEntity*>(&proto_update);
  syncable::Id server_id = update.id();
  const std::string name = SyncerProtoUtil::NameFromSyncEntity(update);

  // Look to see if there's a local item that should recieve this update,
  // maybe due to a duplicate client tag or a lost commit response.
  syncable::Id local_id = SyncerUtil::FindLocalIdToUpdate(trans, update);

  // FindLocalEntryToUpdate has veto power.
  if (local_id.IsNull()) {
    return SUCCESS_PROCESSED;  // The entry has become irrelevant.
  }

  SyncerUtil::CreateNewEntry(trans, local_id);

  // We take a two step approach. First we store the entries data in the
  // server fields of a local entry and then move the data to the local fields
  syncable::MutableEntry target_entry(trans, syncable::GET_BY_ID, local_id);

  // We need to run the Verify checks again; the world could have changed
  // since VerifyUpdatesCommand.
  if (!ReverifyEntry(trans, update, &target_entry)) {
    return SUCCESS_PROCESSED;  // The entry has become irrelevant.
  }

  // If we're repurposing an existing local entry with a new server ID,
  // change the ID now, after we're sure that the update can succeed.
  if (local_id != server_id) {
    DCHECK(!update.deleted());
    ChangeEntryIDAndUpdateChildren(trans, &target_entry, server_id);
    // When IDs change, versions become irrelevant.  Forcing BASE_VERSION
    // to zero would ensure that this update gets applied, but would indicate
    // creation or undeletion if it were committed that way.  Instead, prefer
    // forcing BASE_VERSION to entry.version() while also forcing
    // IS_UNAPPLIED_UPDATE to true.  If the item is UNSYNCED, it's committable
    // from the new state; it may commit before the conflict resolver gets
    // a crack at it.
    if (target_entry.Get(syncable::IS_UNSYNCED) ||
        target_entry.Get(syncable::BASE_VERSION) > 0) {
      // If either of these conditions are met, then we can expect valid client
      // fields for this entry.  When BASE_VERSION is positive, consistency is
      // enforced on the client fields at update-application time.  Otherwise,
      // we leave the BASE_VERSION field alone; it'll get updated the first time
      // we successfully apply this update.
      target_entry.Put(syncable::BASE_VERSION, update.version());
    }
    // Force application of this update, no matter what.
    target_entry.Put(syncable::IS_UNAPPLIED_UPDATE, true);
  }

  // If this is a newly received undecryptable update, and the only thing that
  // has changed are the specifics, store the original decryptable specifics,
  // (on which any current or future local changes are based) before we
  // overwrite SERVER_SPECIFICS.
  // MTIME, CTIME, and NON_UNIQUE_NAME are not enforced.
  if (!update.deleted() && !target_entry.Get(syncable::SERVER_IS_DEL) &&
      (update.parent_id() == target_entry.Get(syncable::SERVER_PARENT_ID)) &&
      (update.position_in_parent() ==
          target_entry.Get(syncable::SERVER_POSITION_IN_PARENT)) &&
      update.has_specifics() && update.specifics().has_encrypted() &&
      !cryptographer->CanDecrypt(update.specifics().encrypted())) {
    sync_pb::EntitySpecifics prev_specifics =
        target_entry.Get(syncable::SERVER_SPECIFICS);
    // We only store the old specifics if they were decryptable and applied and
    // there is no BASE_SERVER_SPECIFICS already. Else do nothing.
    if (!target_entry.Get(syncable::IS_UNAPPLIED_UPDATE) &&
        !syncable::IsRealDataType(syncable::GetModelTypeFromSpecifics(
            target_entry.Get(syncable::BASE_SERVER_SPECIFICS))) &&
        (!prev_specifics.has_encrypted() ||
         cryptographer->CanDecrypt(prev_specifics.encrypted()))) {
      DVLOG(2) << "Storing previous server specifcs: "
               << prev_specifics.SerializeAsString();
      target_entry.Put(syncable::BASE_SERVER_SPECIFICS, prev_specifics);
    }
  } else if (syncable::IsRealDataType(syncable::GetModelTypeFromSpecifics(
                 target_entry.Get(syncable::BASE_SERVER_SPECIFICS)))) {
    // We have a BASE_SERVER_SPECIFICS, but a subsequent non-specifics-only
    // change arrived. As a result, we can't use the specifics alone to detect
    // changes, so we clear BASE_SERVER_SPECIFICS.
    target_entry.Put(syncable::BASE_SERVER_SPECIFICS,
                     sync_pb::EntitySpecifics());
  }

  SyncerUtil::UpdateServerFieldsFromUpdate(&target_entry, update, name);

  return SUCCESS_PROCESSED;
}

}  // namespace browser_sync
