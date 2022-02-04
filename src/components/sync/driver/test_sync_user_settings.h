// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_
#define COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
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

  bool IsFirstSetupComplete() const override;
  void SetFirstSetupComplete(SyncFirstSetupCompleteSource source) override;

  bool IsSyncEverythingEnabled() const override;
  UserSelectableTypeSet GetSelectedTypes() const override;
  void SetSelectedTypes(bool sync_everything,
                        UserSelectableTypeSet types) override;
  UserSelectableTypeSet GetRegisteredSelectableTypes() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool IsSyncAllOsTypesEnabled() const override;
  UserSelectableOsTypeSet GetSelectedOsTypes() const override;
  void SetSelectedOsTypes(bool sync_all_os_types,
                          UserSelectableOsTypeSet types) override;
  UserSelectableOsTypeSet GetRegisteredSelectableOsTypes() const override;
#endif

  bool IsCustomPassphraseAllowed() const override;
  bool IsEncryptEverythingEnabled() const override;

  syncer::ModelTypeSet GetEncryptedDataTypes() const override;
  bool IsPassphraseRequired() const override;
  bool IsPassphraseRequiredForPreferredDataTypes() const override;
  bool IsPassphrasePromptMutedForCurrentProductVersion() const override;
  void MarkPassphrasePromptMutedForCurrentProductVersion() override;
  bool IsTrustedVaultKeyRequired() const override;
  bool IsTrustedVaultKeyRequiredForPreferredDataTypes() const override;
  bool IsTrustedVaultRecoverabilityDegraded() const override;
  bool IsUsingExplicitPassphrase() const override;
  base::Time GetExplicitPassphraseTime() const override;
  PassphraseType GetPassphraseType() const override;

  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;

  void SetFirstSetupComplete();
  void ClearFirstSetupComplete();
  void SetCustomPassphraseAllowed(bool allowed);
  void SetPassphraseRequired(bool required);
  void SetPassphraseRequiredForPreferredDataTypes(bool required);
  void SetTrustedVaultKeyRequired(bool required);
  void SetTrustedVaultKeyRequiredForPreferredDataTypes(bool required);
  void SetTrustedVaultRecoverabilityDegraded(bool degraded);
  void SetIsUsingExplicitPassphrase(bool enabled);

 private:
  raw_ptr<TestSyncService> service_;

  bool first_setup_complete_ = true;
  bool sync_everything_enabled_ = true;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool sync_all_os_types_enabled_ = true;
#endif

  bool passphrase_required_ = false;
  bool passphrase_required_for_preferred_data_types_ = false;
  bool trusted_vault_key_required_ = false;
  bool trusted_vault_key_required_for_preferred_data_types_ = false;
  bool trusted_vault_recoverability_degraded_ = false;
  bool using_explicit_passphrase_ = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_TEST_SYNC_USER_SETTINGS_H_
