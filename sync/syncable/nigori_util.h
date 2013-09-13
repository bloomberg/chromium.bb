// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Various utility methods for nigori-based multi-type encryption.

#ifndef SYNC_SYNCABLE_NIGORI_UTIL_H_
#define SYNC_SYNCABLE_NIGORI_UTIL_H_

#include "base/compiler_specific.h"
#include "sync/base/sync_export.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/protocol/nigori_specifics.pb.h"

namespace sync_pb {
class EntitySpecifics;
}

namespace syncer {
class Cryptographer;

namespace syncable {

const char kEncryptedString[] = "encrypted";

class BaseTransaction;
class Entry;
class MutableEntry;
class WriteTransaction;

// Check if our unsyced changes are encrypted if they need to be based on
// |encrypted_types|.
// Returns: true if all unsynced data that should be encrypted is.
//          false if some unsynced changes need to be encrypted.
// This method is similar to ProcessUnsyncedChangesForEncryption but does not
// modify the data and does not care if data is unnecessarily encrypted.
SYNC_EXPORT_PRIVATE bool VerifyUnsyncedChangesAreEncrypted(
    BaseTransaction* const trans,
    ModelTypeSet encrypted_types);

// Processes all unsynced changes and ensures they are appropriately encrypted
// or unencrypted, based on |encrypted_types|.
bool ProcessUnsyncedChangesForEncryption(
    WriteTransaction* const trans);

// Returns true if the entry requires encryption but is not encrypted, false
// otherwise. Note: this does not check that already encrypted entries are
// encrypted with the proper key.
bool EntryNeedsEncryption(ModelTypeSet encrypted_types,
                          const Entry& entry);

// Same as EntryNeedsEncryption, but looks at specifics.
SYNC_EXPORT_PRIVATE bool SpecificsNeedsEncryption(
    ModelTypeSet encrypted_types,
    const sync_pb::EntitySpecifics& specifics);

// Verifies all data of type |type| is encrypted appropriately.
SYNC_EXPORT_PRIVATE bool VerifyDataTypeEncryptionForTest(
    BaseTransaction* const trans,
    ModelType type,
    bool is_encrypted) WARN_UNUSED_RESULT;

// Stores |new_specifics| into |entry|, encrypting if necessary.
// Returns false if an error encrypting occurred (does not modify |entry|).
// Note: gracefully handles new_specifics aliasing with entry->GetSpecifics().
bool UpdateEntryWithEncryption(
    BaseTransaction* const trans,
    const sync_pb::EntitySpecifics& new_specifics,
    MutableEntry* entry);

// Updates |nigori| to match the encryption state specified by |encrypted_types|
// and |encrypt_everything|.
SYNC_EXPORT_PRIVATE void UpdateNigoriFromEncryptedTypes(
    ModelTypeSet encrypted_types,
    bool encrypt_everything,
    sync_pb::NigoriSpecifics* nigori);

// Extracts the set of encrypted types from a nigori node.
ModelTypeSet GetEncryptedTypesFromNigori(
    const sync_pb::NigoriSpecifics& nigori);

}  // namespace syncable
}  // namespace syncer

#endif  // SYNC_SYNCABLE_NIGORI_UTIL_H_
