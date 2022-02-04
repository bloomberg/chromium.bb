// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_IMPL_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_IMPL_H_

#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_type_preference_provider.h"
#include "components/sync/driver/sync_user_settings.h"

namespace syncer {

class SyncPrefs;
class SyncServiceCrypto;

class SyncUserSettingsImpl : public SyncUserSettings {
 public:
  // Both |crypto| and |prefs| must not be null, and must outlive this object.
  // |preference_provider| can be null, but must outlive this object if not
  // null.
  SyncUserSettingsImpl(SyncServiceCrypto* crypto,
                       SyncPrefs* prefs,
                       const SyncTypePreferenceProvider* preference_provider,
                       ModelTypeSet registered_types);
  ~SyncUserSettingsImpl() override;

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

  ModelTypeSet GetEncryptedDataTypes() const override;
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

  void SetSyncRequestedIfNotSetExplicitly();

  ModelTypeSet GetPreferredDataTypes() const;
  bool IsEncryptedDatatypeEnabled() const;

  // Converts |selected_types| to ModelTypeSet of corresponding UserTypes() by
  // resolving pref groups (e.g. {kExtensions} becomes {EXTENSIONS,
  // EXTENSION_SETTINGS}).
  static ModelTypeSet ResolvePreferredTypesForTesting(
      UserSelectableTypeSet selected_types);

 private:
  const raw_ptr<SyncServiceCrypto> crypto_;
  const raw_ptr<SyncPrefs> prefs_;
  const raw_ptr<const SyncTypePreferenceProvider> preference_provider_;
  const ModelTypeSet registered_model_types_;
  base::RepeatingCallback<void(bool)> sync_allowed_by_platform_changed_cb_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_USER_SETTINGS_IMPL_H_
