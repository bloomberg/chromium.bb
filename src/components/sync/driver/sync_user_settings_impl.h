// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_IMPL_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_IMPL_H_

#include <string>

#include "base/callback.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_user_settings.h"

namespace syncer {

class SyncPrefs;
class SyncServiceCrypto;

class SyncUserSettingsImpl : public SyncUserSettings {
 public:
  // Both |crypto| and |prefs| must not be null, and must outlive this object.
  SyncUserSettingsImpl(SyncServiceCrypto* crypto,
                       SyncPrefs* prefs,
                       ModelTypeSet registered_types,
                       const base::RepeatingCallback<void(bool)>&
                           sync_allowed_by_platform_changed);
  ~SyncUserSettingsImpl() override;

  bool IsSyncRequested() const override;
  void SetSyncRequested(bool requested) override;

  bool IsSyncAllowedByPlatform() const override;
  void SetSyncAllowedByPlatform(bool allowed) override;

  bool IsFirstSetupComplete() const override;
  void SetFirstSetupComplete() override;

  bool IsSyncEverythingEnabled() const override;
  ModelTypeSet GetChosenDataTypes() const override;
  void SetChosenDataTypes(bool sync_everything, ModelTypeSet types) override;

  bool IsEncryptEverythingAllowed() const override;
  void SetEncryptEverythingAllowed(bool allowed) override;
  bool IsEncryptEverythingEnabled() const override;
  void EnableEncryptEverything() override;

  ModelTypeSet GetEncryptedDataTypes() const override;
  bool IsPassphraseRequired() const override;
  bool IsPassphraseRequiredForDecryption() const override;
  bool IsUsingSecondaryPassphrase() const override;
  base::Time GetExplicitPassphraseTime() const override;
  PassphraseType GetPassphraseType() const override;

  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;

  ModelTypeSet GetPreferredDataTypes() const;
  bool IsEncryptedDatatypeEnabled() const;
  bool IsEncryptionPending() const;

 private:
  SyncServiceCrypto* const crypto_;
  SyncPrefs* const prefs_;
  const ModelTypeSet registered_types_;
  base::RepeatingCallback<void(bool)> sync_allowed_by_platform_changed_cb_;

  // Whether sync is currently allowed on this platform.
  bool sync_allowed_by_platform_ = true;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_IMPL_H_
