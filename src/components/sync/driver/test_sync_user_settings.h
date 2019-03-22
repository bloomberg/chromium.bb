// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_
#define COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_

#include <string>

#include "components/sync/driver/sync_user_settings.h"

namespace syncer {

class TestSyncService;

// Test implementation of SyncUserSettings that mostly forwards calls to a
// TestSyncService.
class TestSyncUserSettings : public SyncUserSettings {
 public:
  explicit TestSyncUserSettings(TestSyncService* service);
  ~TestSyncUserSettings() override;

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

  bool IsPassphraseRequired() const override;
  bool IsPassphraseRequiredForDecryption() const override;
  bool IsUsingSecondaryPassphrase() const override;
  base::Time GetExplicitPassphraseTime() const override;
  PassphraseType GetPassphraseType() const override;

  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;

  void SetFirstSetupComplete(bool first_setup_complete);

 private:
  TestSyncService* service_;

  bool first_setup_complete_ = true;
  bool sync_everything_enabled_ = true;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_
