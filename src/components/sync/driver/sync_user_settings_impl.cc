// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_user_settings_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service_crypto.h"
#include "components/version_info/version_info.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

namespace syncer {

namespace {

ModelTypeSet ResolvePreferredTypes(UserSelectableTypeSet selected_types) {
  ModelTypeSet preferred_types;
  for (UserSelectableType type : selected_types) {
    preferred_types.PutAll(UserSelectableTypeToAllModelTypes(type));
  }
  return preferred_types;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
ModelTypeSet ResolvePreferredOsTypes(UserSelectableOsTypeSet selected_types) {
  ModelTypeSet preferred_types;
  for (UserSelectableOsType type : selected_types) {
    preferred_types.PutAll(UserSelectableOsTypeToAllModelTypes(type));
  }
  return preferred_types;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

int GetCurrentMajorProductVersion() {
  DCHECK(version_info::GetVersion().IsValid());
  return version_info::GetVersion().components()[0];
}

}  // namespace

SyncUserSettingsImpl::SyncUserSettingsImpl(
    SyncServiceCrypto* crypto,
    SyncPrefs* prefs,
    const SyncTypePreferenceProvider* preference_provider,
    ModelTypeSet registered_model_types)
    : crypto_(crypto),
      prefs_(prefs),
      preference_provider_(preference_provider),
      registered_model_types_(registered_model_types) {
  DCHECK(crypto_);
  DCHECK(prefs_);
}

SyncUserSettingsImpl::~SyncUserSettingsImpl() = default;

bool SyncUserSettingsImpl::IsSyncRequested() const {
  return prefs_->IsSyncRequested();
}

void SyncUserSettingsImpl::SetSyncRequested(bool requested) {
  prefs_->SetSyncRequested(requested);
}

bool SyncUserSettingsImpl::IsFirstSetupComplete() const {
  return prefs_->IsFirstSetupComplete();
}

void SyncUserSettingsImpl::SetFirstSetupComplete(
    SyncFirstSetupCompleteSource source) {
  if (IsFirstSetupComplete())
    return;
  UMA_HISTOGRAM_ENUMERATION("Signin.SyncFirstSetupCompleteSource", source);
  prefs_->SetFirstSetupComplete();
}

bool SyncUserSettingsImpl::IsSyncEverythingEnabled() const {
  return prefs_->HasKeepEverythingSynced();
}

UserSelectableTypeSet SyncUserSettingsImpl::GetSelectedTypes() const {
  UserSelectableTypeSet types = prefs_->GetSelectedTypes();
  types.RetainAll(GetRegisteredSelectableTypes());
  return types;
}

void SyncUserSettingsImpl::SetSelectedTypes(bool sync_everything,
                                            UserSelectableTypeSet types) {
  UserSelectableTypeSet registered_types = GetRegisteredSelectableTypes();
  DCHECK(registered_types.HasAll(types))
      << "\n registered: " << UserSelectableTypeSetToString(registered_types)
      << "\n setting to: " << UserSelectableTypeSetToString(types);
  prefs_->SetSelectedTypes(sync_everything, registered_types, types);
}

UserSelectableTypeSet SyncUserSettingsImpl::GetRegisteredSelectableTypes()
    const {
  UserSelectableTypeSet registered_types;
  for (UserSelectableType type : UserSelectableTypeSet::All()) {
    if (registered_model_types_.Has(
            UserSelectableTypeToCanonicalModelType(type))) {
      registered_types.Put(type);
    }
  }
  return registered_types;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool SyncUserSettingsImpl::IsSyncAllOsTypesEnabled() const {
  DCHECK(chromeos::features::IsSyncSettingsCategorizationEnabled());
  return prefs_->IsSyncAllOsTypesEnabled();
}

UserSelectableOsTypeSet SyncUserSettingsImpl::GetSelectedOsTypes() const {
  DCHECK(chromeos::features::IsSyncSettingsCategorizationEnabled());
  UserSelectableOsTypeSet types = prefs_->GetSelectedOsTypes();
  types.RetainAll(GetRegisteredSelectableOsTypes());
  return types;
}

void SyncUserSettingsImpl::SetSelectedOsTypes(bool sync_all_os_types,
                                              UserSelectableOsTypeSet types) {
  DCHECK(chromeos::features::IsSyncSettingsCategorizationEnabled());
  UserSelectableOsTypeSet registered_types = GetRegisteredSelectableOsTypes();
  DCHECK(registered_types.HasAll(types));
  prefs_->SetSelectedOsTypes(sync_all_os_types, registered_types, types);
}

UserSelectableOsTypeSet SyncUserSettingsImpl::GetRegisteredSelectableOsTypes()
    const {
  DCHECK(chromeos::features::IsSyncSettingsCategorizationEnabled());
  UserSelectableOsTypeSet registered_types;
  for (UserSelectableOsType type : UserSelectableOsTypeSet::All()) {
    if (registered_model_types_.Has(
            UserSelectableOsTypeToCanonicalModelType(type))) {
      registered_types.Put(type);
    }
  }
  return registered_types;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool SyncUserSettingsImpl::IsCustomPassphraseAllowed() const {
  return !preference_provider_ ||
         preference_provider_->IsCustomPassphraseAllowed();
}

bool SyncUserSettingsImpl::IsEncryptEverythingEnabled() const {
  return crypto_->IsEncryptEverythingEnabled();
}

bool SyncUserSettingsImpl::IsPassphraseRequired() const {
  return crypto_->IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsPassphraseRequiredForPreferredDataTypes() const {
  // If there is an encrypted datatype enabled and we don't have the proper
  // passphrase, we must prompt the user for a passphrase. The only way for the
  // user to avoid entering their passphrase is to disable the encrypted types.
  return IsEncryptedDatatypeEnabled() && IsPassphraseRequired();
}

bool SyncUserSettingsImpl::IsPassphrasePromptMutedForCurrentProductVersion()
    const {
  return prefs_->GetPassphrasePromptMutedProductVersion() ==
         GetCurrentMajorProductVersion();
}

void SyncUserSettingsImpl::MarkPassphrasePromptMutedForCurrentProductVersion() {
  prefs_->SetPassphrasePromptMutedProductVersion(
      GetCurrentMajorProductVersion());
}

bool SyncUserSettingsImpl::IsTrustedVaultKeyRequired() const {
  return crypto_->IsTrustedVaultKeyRequired();
}

bool SyncUserSettingsImpl::IsTrustedVaultKeyRequiredForPreferredDataTypes()
    const {
  return IsEncryptedDatatypeEnabled() && crypto_->IsTrustedVaultKeyRequired();
}

bool SyncUserSettingsImpl::IsTrustedVaultRecoverabilityDegraded() const {
  return IsEncryptedDatatypeEnabled() &&
         crypto_->IsTrustedVaultRecoverabilityDegraded();
}

bool SyncUserSettingsImpl::IsUsingExplicitPassphrase() const {
  return crypto_->IsUsingExplicitPassphrase();
}

base::Time SyncUserSettingsImpl::GetExplicitPassphraseTime() const {
  return crypto_->GetExplicitPassphraseTime();
}

PassphraseType SyncUserSettingsImpl::GetPassphraseType() const {
  return crypto_->GetPassphraseType();
}

void SyncUserSettingsImpl::SetEncryptionPassphrase(
    const std::string& passphrase) {
  crypto_->SetEncryptionPassphrase(passphrase);
}

bool SyncUserSettingsImpl::SetDecryptionPassphrase(
    const std::string& passphrase) {
  DCHECK(IsPassphraseRequired())
      << "SetDecryptionPassphrase must not be called when "
         "IsPassphraseRequired() is false.";

  DVLOG(1) << "Setting passphrase for decryption.";

  return crypto_->SetDecryptionPassphrase(passphrase);
}

void SyncUserSettingsImpl::SetSyncRequestedIfNotSetExplicitly() {
  prefs_->SetSyncRequestedIfNotSetExplicitly();
}

ModelTypeSet SyncUserSettingsImpl::GetPreferredDataTypes() const {
  ModelTypeSet types = ResolvePreferredTypes(GetSelectedTypes());
  types.PutAll(AlwaysPreferredUserTypes());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (chromeos::features::IsSyncSettingsCategorizationEnabled()) {
    types.PutAll(ResolvePreferredOsTypes(GetSelectedOsTypes()));
  }
#endif
  types.RetainAll(registered_model_types_);

  static_assert(38 == GetNumModelTypes(),
                "If adding a new sync data type, update the list below below if"
                " you want to disable the new data type for local sync.");
  types.PutAll(ControlTypes());
  if (prefs_->IsLocalSyncEnabled()) {
    types.Remove(APP_LIST);
    types.Remove(AUTOFILL_WALLET_OFFER);
    types.Remove(SECURITY_EVENTS);
    types.Remove(SEND_TAB_TO_SELF);
    types.Remove(SHARING_MESSAGE);
    types.Remove(USER_CONSENTS);
    types.Remove(USER_EVENTS);
    types.Remove(WORKSPACE_DESK);
  }
  return types;
}

ModelTypeSet SyncUserSettingsImpl::GetEncryptedDataTypes() const {
  return crypto_->GetEncryptedDataTypes();
}

bool SyncUserSettingsImpl::IsEncryptedDatatypeEnabled() const {
  const ModelTypeSet preferred_types = GetPreferredDataTypes();
  const ModelTypeSet encrypted_types = GetEncryptedDataTypes();
  DCHECK(encrypted_types.HasAll(AlwaysEncryptedUserTypes()));
  return !Intersection(preferred_types, encrypted_types).Empty();
}

// static
ModelTypeSet SyncUserSettingsImpl::ResolvePreferredTypesForTesting(
    UserSelectableTypeSet selected_types) {
  return ResolvePreferredTypes(selected_types);
}

}  // namespace syncer
