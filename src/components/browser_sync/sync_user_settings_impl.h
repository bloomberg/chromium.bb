// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_USER_SETTINGS_IMPL_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_USER_SETTINGS_IMPL_H_

#include <string>

#include "components/sync/driver/sync_user_settings.h"

namespace syncer {
class SyncPrefs;
}

namespace browser_sync {

class ProfileSyncService;

class SyncUserSettingsImpl : public syncer::SyncUserSettings {
 public:
  // Both |service| and |prefs| must not be null, and must outlive this object.
  SyncUserSettingsImpl(ProfileSyncService* service, syncer::SyncPrefs* prefs);
  ~SyncUserSettingsImpl() override;

  bool IsSyncRequested() const override;
  void SetSyncRequested(bool requested) override;

  bool IsSyncAllowedByPlatform() const override;
  void SetSyncAllowedByPlatform(bool allowed) override;

  bool IsFirstSetupComplete() const override;
  void SetFirstSetupComplete() override;

  bool IsSyncEverythingEnabled() const override;
  syncer::ModelTypeSet GetChosenDataTypes() const override;
  void SetChosenDataTypes(bool sync_everything,
                          syncer::ModelTypeSet types) override;

  bool IsEncryptEverythingAllowed() const override;
  void SetEncryptEverythingAllowed(bool allowed) override;
  bool IsEncryptEverythingEnabled() const override;
  void EnableEncryptEverything() override;

  bool IsPassphraseRequired() const override;
  bool IsPassphraseRequiredForDecryption() const override;
  bool IsUsingSecondaryPassphrase() const override;
  base::Time GetExplicitPassphraseTime() const override;
  syncer::PassphraseType GetPassphraseType() const override;

  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;

 private:
  ProfileSyncService* const service_;
  syncer::SyncPrefs* const prefs_;

  // Whether sync is currently allowed on this platform.
  bool sync_allowed_by_platform_ = true;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_USER_SETTINGS_IMPL_H_
