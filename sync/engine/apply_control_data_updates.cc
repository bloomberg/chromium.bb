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
    // Create a new set of specifics based on the server specifics (which
    // preserves their encryption keys).
    sync_pb::EntitySpecifics specifics = nigori_node.Get(SERVER_SPECIFICS);
    sync_pb::NigoriSpecifics* server_nigori = specifics.mutable_nigori();
    // Store the merged set of encrypted types.
    // (NigoriHandler::ApplyNigoriUpdate(..) will have merged the local types
    // already).
    trans->directory()->GetNigoriHandler()->UpdateNigoriFromEncryptedTypes(
        server_nigori,
        trans);
    // The cryptographer has the both the local and remote encryption keys
    // (added at NigoriHandler::ApplyNigoriUpdate(..) time).
    // If the cryptographer is ready, then it already merged both sets of keys
    // and we can store them back in. In that case, the remote key was already
    // part of the local keybag, so we preserve the local key as the default
    // (including whether it's an explicit key).
    // If the cryptographer is not ready, then the user will have to provide
    // the passphrase to decrypt the pending keys. When they do so, the
    // SetDecryptionPassphrase code will act based on whether the server
    // update has an explicit passphrase or not.
    // - If the server had an explicit passphrase, that explicit passphrase
    //   will be preserved as the default encryption key.
    // - If the server did not have an explicit passphrase, we assume the
    //   local passphrase is the most up to date and preserve the local
    //   default encryption key marked as an implicit passphrase.
    // This works fine except for the case where we had locally set an
    // explicit passphrase. In that case the nigori node will have the default
    // key based on the local explicit passphassphrase, but will not have it
    // marked as explicit. To fix this we'd have to track whether we have a
    // explicit passphrase or not separate from the nigori, which would
    // introduce even more complexity, so we leave it up to the user to reset
    // that passphrase as an explicit one via settings. The goal here is to
    // ensure both sets of encryption keys are preserved.
    if (cryptographer->is_ready()) {
      cryptographer->GetKeys(server_nigori->mutable_encryption_keybag());
      server_nigori->set_keybag_is_frozen(
          nigori_node.Get(SPECIFICS).nigori().keybag_is_frozen());
    }
    nigori_node.Put(SPECIFICS, specifics);
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
