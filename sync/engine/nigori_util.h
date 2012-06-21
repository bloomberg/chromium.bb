// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Various utility methods for nigori-based multi-type encryption.

#ifndef SYNC_ENGINE_NIGORI_UTIL_H_
#define SYNC_ENGINE_NIGORI_UTIL_H_
#pragma once

#include "base/compiler_specific.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/protocol/nigori_specifics.pb.h"

namespace csync {
class Cryptographer;
}

namespace sync_pb {
class EntitySpecifics;
}

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
bool VerifyUnsyncedChangesAreEncrypted(
    BaseTransaction* const trans,
    ModelTypeSet encrypted_types);

// Processes all unsynced changes and ensures they are appropriately encrypted
// or unencrypted, based on |encrypted_types|.
bool ProcessUnsyncedChangesForEncryption(
    WriteTransaction* const trans,
    csync::Cryptographer* cryptographer);

// Returns true if the entry requires encryption but is not encrypted, false
// otherwise. Note: this does not check that already encrypted entries are
// encrypted with the proper key.
bool EntryNeedsEncryption(ModelTypeSet encrypted_types,
                          const Entry& entry);

// Same as EntryNeedsEncryption, but looks at specifics.
bool SpecificsNeedsEncryption(ModelTypeSet encrypted_types,
                              const sync_pb::EntitySpecifics& specifics);

// Verifies all data of type |type| is encrypted appropriately.
bool VerifyDataTypeEncryptionForTest(
    BaseTransaction* const trans,
    csync::Cryptographer* cryptographer,
    ModelType type,
    bool is_encrypted) WARN_UNUSED_RESULT;

// Stores |new_specifics| into |entry|, encrypting if necessary.
// Returns false if an error encrypting occurred (does not modify |entry|).
// Note: gracefully handles new_specifics aliasing with entry->Get(SPECIFICS).
bool UpdateEntryWithEncryption(
    csync::Cryptographer* cryptographer,
    const sync_pb::EntitySpecifics& new_specifics,
    MutableEntry* entry);

}  // namespace syncable

#endif  // SYNC_ENGINE_NIGORI_UTIL_H_
