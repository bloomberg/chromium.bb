// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/data_type_encryption_handler.h"

namespace syncer {

// This class encapsulates all the user-configurable bits of Sync.
class SyncUserSettings : public syncer::DataTypeEncryptionHandler {
 public:
  ~SyncUserSettings() override = default;

  // Whether the user wants Sync to run, a.k.a. the Sync feature toggle in
  // settings. This maps to DISABLE_REASON_USER_CHOICE.
  // NOTE: This is true by default, even if the user has never enabled Sync or
  // isn't even signed in.
  virtual bool IsSyncRequested() const = 0;
  virtual void SetSyncRequested(bool requested) = 0;

  // Whether Sync is allowed at the platform level (e.g. Android's "MasterSync"
  // toggle). Maps to DISABLE_REASON_PLATFORM_OVERRIDE.
  virtual bool IsSyncAllowedByPlatform() const = 0;
  virtual void SetSyncAllowedByPlatform(bool allowed) = 0;

  // Whether the initial Sync setup has been completed, meaning the user has
  // consented to Sync.
  // NOTE: On Android and ChromeOS, this gets set automatically, so it doesn't
  // really mean anything. See |browser_defaults::kSyncAutoStarts|.
  virtual bool IsFirstSetupComplete() const = 0;
  virtual void SetFirstSetupComplete() = 0;

  // The user's selected types. The "sync everything" flag means to sync all
  // current and future data types. If it is set, then GetSelectedTypes() will
  // always return "all types".
  // NOTE: By default, GetSelectedTypes() returns "all types", even if the user
  // has never enabled Sync, or if only Sync-the-transport is running.
  virtual bool IsSyncEverythingEnabled() const = 0;
  virtual UserSelectableTypeSet GetSelectedTypes() const = 0;
  virtual void SetSelectedTypes(bool sync_everything,
                                UserSelectableTypeSet types) = 0;
  // Registered user selectable types are derived from registered model types.
  // UserSelectableType is registered iff main corresponding  ModelType is
  // registered.
  virtual UserSelectableTypeSet GetRegisteredSelectableTypes() const = 0;
  // Returns the set of types which are enforced programmatically and can not
  // be disabled by the user (e.g. enforced for supervised users). Types are
  // not guaranteed to be registered.
  virtual UserSelectableTypeSet GetForcedTypes() const = 0;

  // Encryption state.
  // Note that all of this state may only be queried or modified if the Sync
  // engine is initialized.

  // Whether the user is allowed to encrypt all their Sync data. For example,
  // child accounts are not allowed to encrypt their data.
  virtual bool IsEncryptEverythingAllowed() const = 0;
  // Whether we are currently set to encrypt all the Sync data.
  virtual bool IsEncryptEverythingEnabled() const = 0;
  // Turns on encryption for all data. Callers must call SetChosenDataTypes()
  // after calling this to force the encryption to occur.
  virtual void EnableEncryptEverything() = 0;

  // The current set of encrypted data types.
  ModelTypeSet GetEncryptedDataTypes() const override = 0;
  // Whether a passphrase is required for encryption or decryption to proceed.
  // Note that Sync might still be working fine if the user has disabled all
  // encrypted data types.
  bool IsPassphraseRequired() const override = 0;
  // Whether a passphrase is required to decrypt the data for any currently
  // enabled data type.
  virtual bool IsPassphraseRequiredForDecryption() const = 0;
  // Whether a "secondary" passphrase is in use, which means either a custom or
  // a frozen implicit passphrase.
  virtual bool IsUsingSecondaryPassphrase() const = 0;
  // The time the current explicit passphrase (if any) was set. If no secondary
  // passphrase is in use, or no time is available, returns an unset base::Time.
  virtual base::Time GetExplicitPassphraseTime() const = 0;
  // The type of the passphrase currently in use. This is KEYSTORE_PASSPHRASE if
  // "encrypt everything" is disabled, or CUSTOM_PASSPHRASE if
  // "encrypt everything" is enabled. There are also some legacy passphrase
  // types which may still occur for a small number of users.
  virtual PassphraseType GetPassphraseType() const = 0;

  // Asynchronously sets the passphrase to |passphrase| for encryption.
  virtual void SetEncryptionPassphrase(const std::string& passphrase) = 0;
  // Asynchronously decrypts pending keys using |passphrase|. Returns false
  // immediately if the passphrase could not be used to decrypt a locally cached
  // copy of encrypted keys; returns true otherwise.
  virtual bool SetDecryptionPassphrase(const std::string& passphrase)
      WARN_UNUSED_RESULT = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_H_
