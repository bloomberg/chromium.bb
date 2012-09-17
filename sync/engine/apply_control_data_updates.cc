// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/apply_control_data_updates.h"

#include "base/metrics/histogram.h"
#include "sync/engine/conflict_resolver.h"
#include "sync/engine/conflict_util.h"
#include "sync/engine/syncer_util.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/nigori_handler.h"
#include "sync/syncable/nigori_util.h"
#include "sync/syncable/write_transaction.h"
#include "sync/util/cryptographer.h"

namespace syncer {

using syncable::GET_BY_SERVER_TAG;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::SERVER_SPECIFICS;
using syncable::SPECIFICS;
using syncable::SYNCER;

void ApplyControlDataUpdates(syncable::Directory* dir) {
  syncable::WriteTransaction trans(FROM_HERE, SYNCER, dir);

  if (ApplyNigoriUpdates(&trans, dir->GetCryptographer(&trans))) {
    dir->set_initial_sync_ended_for_type(NIGORI, true);
  }
}

// Update the sync encryption handler with the server's nigori node.
//
// If we have a locally modified nigori node, we merge them manually. This
// handles the case where two clients both set a different passphrase. The
// second client to attempt to commit will go into a state of having pending
// keys, unioned the set of encrypted types, and eventually re-encrypt
// everything with the passphrase of the first client and commit the set of
// merged encryption keys. Until the second client provides the pending
// passphrase, the cryptographer will preserve the encryption keys based on the
// local passphrase, while the nigori node will preserve the server encryption
// keys.
bool ApplyNigoriUpdates(syncable::WriteTransaction* trans,
                        Cryptographer* cryptographer) {
  syncable::MutableEntry nigori_node(trans, GET_BY_SERVER_TAG,
                                     ModelTypeToRootTag(NIGORI));

  // Mainly for unit tests.  We should have a Nigori node by this point.
  if (!nigori_node.good()) {
    return false;
  }

  if (!nigori_node.Get(IS_UNAPPLIED_UPDATE)) {
    return true;
  }

  // We apply the nigori update regardless of whether there's a conflict or
  // not in order to preserve any new encrypted types or encryption keys.
  const sync_pb::NigoriSpecifics& nigori =
      nigori_node.Get(SERVER_SPECIFICS).nigori();
  trans->directory()->GetNigoriHandler()->ApplyNigoriUpdate(nigori, trans);

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
    syncable::ProcessUnsyncedChangesForEncryption(trans);
  }

  if (!nigori_node.Get(IS_UNSYNCED)) {  // Update only.
    UpdateLocalDataFromServerData(trans, &nigori_node);
  } else {  // Conflict.
    const sync_pb::EntitySpecifics& server_specifics =
        nigori_node.Get(SERVER_SPECIFICS);
    const sync_pb::NigoriSpecifics& server_nigori = server_specifics.nigori();
    const sync_pb::EntitySpecifics& local_specifics =
        nigori_node.Get(SPECIFICS);
    const sync_pb::NigoriSpecifics& local_nigori = local_specifics.nigori();

    // We initialize the new nigori with the server state, and will override
    // it as necessary below.
    sync_pb::EntitySpecifics new_specifics = nigori_node.Get(SERVER_SPECIFICS);
    sync_pb::NigoriSpecifics* new_nigori = new_specifics.mutable_nigori();

    // If the cryptographer is not ready, another client set a new encryption
    // passphrase. If we had migrated locally, we will re-migrate when the
    // pending keys are provided. If we had set a new custom passphrase locally
    // the user will have another chance to set a custom passphrase later
    // (assuming they hadn't set a custom passphrase on the other client).
    // Therefore, we only attempt to merge the nigori nodes if the cryptographer
    // is ready.
    // Note: we only update the encryption keybag if we're sure that we aren't
    // invalidating the keystore_decryptor_token (i.e. we're either
    // not migrated or we copying over all local state).
    if (cryptographer->is_ready()) {
      if (local_nigori.has_passphrase_type() &&
          server_nigori.has_passphrase_type()) {
        // They're both migrated, preserve the local nigori if the passphrase
        // type is more conservative.
        if (server_nigori.passphrase_type() ==
                sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE &&
            local_nigori.passphrase_type() !=
                sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE) {
          DCHECK(local_nigori.passphrase_type() ==
                     sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE ||
                 local_nigori.passphrase_type() ==
                     sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
          new_nigori->CopyFrom(local_nigori);
          cryptographer->GetKeys(new_nigori->mutable_encryption_keybag());
        }
      } else if (!local_nigori.has_passphrase_type() &&
                 !server_nigori.has_passphrase_type()) {
        // Set the explicit passphrase based on the local state. If the server
        // had set an explict passphrase, we should have pending keys, so
        // should not reach this code.
        // Because neither side is migrated, we don't have to worry about the
        // keystore decryptor token.
        new_nigori->set_keybag_is_frozen(local_nigori.keybag_is_frozen());
        cryptographer->GetKeys(new_nigori->mutable_encryption_keybag());
      } else if (local_nigori.has_passphrase_type()) {
        // Local is migrated but server is not. Copy over the local migrated
        // data.
        new_nigori->CopyFrom(local_nigori);
        cryptographer->GetKeys(new_nigori->mutable_encryption_keybag());
      }  // else leave the new nigori with the server state.
    }

    // Always update to the safest set of encrypted types.
    trans->directory()->GetNigoriHandler()->UpdateNigoriFromEncryptedTypes(
        new_nigori,
        trans);

    nigori_node.Put(SPECIFICS, new_specifics);
    DVLOG(1) << "Resolving simple conflict, merging nigori nodes: "
             << nigori_node;

    OverwriteServerChanges(&nigori_node);

    UMA_HISTOGRAM_ENUMERATION("Sync.ResolveSimpleConflict",
                              ConflictResolver::NIGORI_MERGE,
                              ConflictResolver::CONFLICT_RESOLUTION_SIZE);
  }

  return true;
}

}  // namespace syncer
