// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_features_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/autofill/core/common/gaia_id_hash.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

using autofill::GaiaIdHash;
using autofill::PasswordForm;
using password_manager::metrics_util::PasswordAccountStorageUsageLevel;
using password_manager::metrics_util::PasswordAccountStorageUserState;

namespace password_manager {
namespace features_util {
namespace {

// Returns whether the account-scoped password storage can be enabled in
// principle for the current profile. This is constant for a given profile
// (until browser restart).
bool CanAccountStorageBeEnabled(const syncer::SyncService* sync_service) {
  if (!base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage)) {
    return false;
  }

  // |sync_service| is null in incognito mode, or if --disable-sync was
  // specified on the command-line.
  if (!sync_service)
    return false;

  return true;
}

// Whether the currently signed-in user (if any) is eligible for using the
// account-scoped password storage. This is the case if:
// - The account storage can be enabled in principle.
// - Sync-the-transport is running (i.e. there's a signed-in user, Sync is not
//   disabled by policy, etc).
// - There is no custom passphrase (because Sync transport offers no way to
//   enter the passphrase yet). Note that checking this requires the SyncEngine
//   to be initialized.
// - Sync-the-feature is NOT enabled (if it is, there's only a single combined
//   storage).
bool IsUserEligibleForAccountStorage(const syncer::SyncService* sync_service) {
  return CanAccountStorageBeEnabled(sync_service) &&
         sync_service->GetTransportState() !=
             syncer::SyncService::TransportState::DISABLED &&
         sync_service->IsEngineInitialized() &&
         !sync_service->GetUserSettings()->IsUsingSecondaryPassphrase() &&
         !sync_service->IsSyncFeatureEnabled();
}

PasswordForm::Store PasswordStoreFromInt(int value) {
  switch (value) {
    case static_cast<int>(PasswordForm::Store::kProfileStore):
      return PasswordForm::Store::kProfileStore;
    case static_cast<int>(PasswordForm::Store::kAccountStore):
      return PasswordForm::Store::kAccountStore;
  }
  return PasswordForm::Store::kNotSet;
}

const char kAccountStorageOptedInKey[] = "opted_in";
const char kAccountStorageDefaultStoreKey[] = "default_store";

// Helper class for reading account storage settings for a given account.
class AccountStorageSettingsReader {
 public:
  AccountStorageSettingsReader(const PrefService* prefs,
                               const GaiaIdHash& gaia_id_hash) {
    const base::DictionaryValue* global_pref =
        prefs->GetDictionary(prefs::kAccountStoragePerAccountSettings);
    if (global_pref)
      account_settings_ = global_pref->FindDictKey(gaia_id_hash.ToBase64());
  }

  bool IsOptedIn() {
    if (!account_settings_)
      return false;
    return account_settings_->FindBoolKey(kAccountStorageOptedInKey)
        .value_or(false);
  }

  PasswordForm::Store GetDefaultStore() const {
    if (!account_settings_)
      return PasswordForm::Store::kNotSet;
    base::Optional<int> value =
        account_settings_->FindIntKey(kAccountStorageDefaultStoreKey);
    if (!value)
      return PasswordForm::Store::kNotSet;
    return PasswordStoreFromInt(*value);
  }

 private:
  // May be null, if no settings for this account were saved yet.
  const base::Value* account_settings_ = nullptr;
};

// Helper class for updating account storage settings for a given account. Like
// with DictionaryPrefUpdate, updates are only published once the instance gets
// destroyed.
class ScopedAccountStorageSettingsUpdate {
 public:
  ScopedAccountStorageSettingsUpdate(PrefService* prefs,
                                     const GaiaIdHash& gaia_id_hash)
      : update_(prefs, prefs::kAccountStoragePerAccountSettings),
        account_hash_(gaia_id_hash.ToBase64()) {}

  base::Value* GetOrCreateAccountSettings() {
    base::Value* account_settings = update_->FindDictKey(account_hash_);
    if (!account_settings) {
      account_settings =
          update_->SetKey(account_hash_, base::DictionaryValue());
    }
    DCHECK(account_settings);
    return account_settings;
  }

  void SetOptedIn() {
    base::Value* account_settings = GetOrCreateAccountSettings();
    account_settings->SetBoolKey(kAccountStorageOptedInKey, true);
  }

  void SetDefaultStore(PasswordForm::Store default_store) {
    base::Value* account_settings = GetOrCreateAccountSettings();
    account_settings->SetIntKey(kAccountStorageDefaultStoreKey,
                                static_cast<int>(default_store));
  }

  void ClearAllSettings() { update_->RemoveKey(account_hash_); }

 private:
  DictionaryPrefUpdate update_;
  const std::string account_hash_;
};
}  // namespace

bool IsOptedInForAccountStorage(const PrefService* pref_service,
                                const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  // If the account storage can't be enabled (e.g. because the feature flag was
  // turned off), then don't consider the user opted in, even if the pref is
  // set.
  // Note: IsUserEligibleForAccountStorage() is not appropriate here, because
  // a) Sync-the-feature users are not considered eligible, but might have
  //    opted in before turning on Sync, and
  // b) eligibility requires IsEngineInitialized() (i.e. will be false for a
  //    few seconds after browser startup).
  if (!CanAccountStorageBeEnabled(sync_service))
    return false;

  // The opt-in is per account, so if there's no account then there's no opt-in.
  std::string gaia_id = sync_service->GetAuthenticatedAccountInfo().gaia;
  if (gaia_id.empty())
    return false;

  return AccountStorageSettingsReader(pref_service,
                                      GaiaIdHash::FromGaiaId(gaia_id))
      .IsOptedIn();
}

bool ShouldShowAccountStorageReSignin(const PrefService* pref_service,
                                      const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  // Checks that the sync_service is not null and the feature is enabled.
  if (!CanAccountStorageBeEnabled(sync_service)) {
    return false;  // Opt-in wouldn't work here, so don't show the re-signin.
  }

  // In order to show a re-signin prompt, no user may be logged in.
  if (!sync_service->HasDisableReason(
          syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN)) {
    return false;
  }

  const base::DictionaryValue* global_pref =
      pref_service->GetDictionary(prefs::kAccountStoragePerAccountSettings);
  // Show the opt-in if any known previous user opted into using the account
  // storage before and might want to access it again.
  return std::any_of(
      global_pref->begin(), global_pref->end(),
      [](const std::pair<std::string, std::unique_ptr<base::Value>>& prefs) {
        return prefs.second->FindBoolKey(kAccountStorageOptedInKey)
            .value_or(false);
      });
}

bool ShouldShowAccountStorageOptIn(const PrefService* pref_service,
                                   const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  // Show the opt-in if the user is eligible, but not yet opted in.
  return IsUserEligibleForAccountStorage(sync_service) &&
         !IsOptedInForAccountStorage(pref_service, sync_service) &&
         !sync_service->IsSyncFeatureEnabled();
}

void OptInToAccountStorage(PrefService* pref_service,
                           const syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  DCHECK(
      base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage));

  std::string gaia_id = sync_service->GetAuthenticatedAccountInfo().gaia;
  if (gaia_id.empty()) {
    // Maybe the account went away since the opt-in UI was shown. This should be
    // rare, but is ultimately harmless - just do nothing here.
    return;
  }
  ScopedAccountStorageSettingsUpdate(pref_service,
                                     GaiaIdHash::FromGaiaId(gaia_id))
      .SetOptedIn();
}

void OptOutOfAccountStorageAndClearSettings(
    PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  DCHECK(
      base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage));

  std::string gaia_id = sync_service->GetAuthenticatedAccountInfo().gaia;
  bool account_exists = !gaia_id.empty();
  base::UmaHistogramBoolean(
      "PasswordManager.AccountStorage.SignedInAccountFoundDuringOptOut",
      account_exists);
  if (!account_exists) {
    // In rare cases, it could happen that the account went away since the
    // opt-out UI was triggered.
    return;
  }
  ScopedAccountStorageSettingsUpdate(pref_service,
                                     GaiaIdHash::FromGaiaId(gaia_id))
      .ClearAllSettings();
}

bool ShouldShowPasswordStorePicker(const PrefService* pref_service,
                                   const syncer::SyncService* sync_service) {
  return !sync_service->IsSyncFeatureEnabled() &&
         (IsOptedInForAccountStorage(pref_service, sync_service) ||
          IsUserEligibleForAccountStorage(sync_service));
}

PasswordForm::Store GetDefaultPasswordStore(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  DCHECK(pref_service);

  if (!IsUserEligibleForAccountStorage(sync_service))
    return PasswordForm::Store::kProfileStore;

  std::string gaia_id = sync_service->GetAuthenticatedAccountInfo().gaia;
  if (gaia_id.empty())
    return PasswordForm::Store::kProfileStore;

  PasswordForm::Store default_store =
      AccountStorageSettingsReader(pref_service,
                                   GaiaIdHash::FromGaiaId(gaia_id))
          .GetDefaultStore();
  // If none of the early-outs above triggered, then we *can* save to the
  // account store in principle (though the user might not have opted in to that
  // yet). In this case, default to the account store.
  if (default_store == PasswordForm::Store::kNotSet)
    return PasswordForm::Store::kAccountStore;
  return default_store;
}

void SetDefaultPasswordStore(PrefService* pref_service,
                             const syncer::SyncService* sync_service,
                             PasswordForm::Store default_store) {
  DCHECK(pref_service);
  DCHECK(sync_service);
  DCHECK(
      base::FeatureList::IsEnabled(features::kEnablePasswordsAccountStorage));

  std::string gaia_id = sync_service->GetAuthenticatedAccountInfo().gaia;
  if (gaia_id.empty()) {
    // Maybe the account went away since the UI was shown. This should be rare,
    // but is ultimately harmless - just do nothing here.
    return;
  }
  ScopedAccountStorageSettingsUpdate(pref_service,
                                     GaiaIdHash::FromGaiaId(gaia_id))
      .SetDefaultStore(default_store);
}

void ClearAccountStorageSettingsForAllUsers(PrefService* pref_service) {
  DCHECK(pref_service);
  pref_service->ClearPref(prefs::kAccountStoragePerAccountSettings);
}

PasswordAccountStorageUserState ComputePasswordAccountStorageUserState(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  DCHECK(pref_service);
  // The SyncService can be null in incognito, or due to a commandline flag. In
  // those cases, simply consider the user as signed out.
  if (!sync_service)
    return PasswordAccountStorageUserState::kSignedOutUser;

  if (sync_service->IsSyncFeatureEnabled())
    return PasswordAccountStorageUserState::kSyncUser;

  if (sync_service->HasDisableReason(
          syncer::SyncService::DisableReason::DISABLE_REASON_NOT_SIGNED_IN)) {
    // Signed out. Check if any account storage opt-in exists.
    return ShouldShowAccountStorageReSignin(pref_service, sync_service)
               ? PasswordAccountStorageUserState::kSignedOutAccountStoreUser
               : PasswordAccountStorageUserState::kSignedOutUser;
  }

  bool saving_locally = GetDefaultPasswordStore(pref_service, sync_service) ==
                        PasswordForm::Store::kProfileStore;

  // Signed in. Check for account storage opt-in.
  if (IsOptedInForAccountStorage(pref_service, sync_service)) {
    // Signed in and opted in. Check default storage location.
    return saving_locally
               ? PasswordAccountStorageUserState::
                     kSignedInAccountStoreUserSavingLocally
               : PasswordAccountStorageUserState::kSignedInAccountStoreUser;
  }

  // Signed in but not opted in. Check default storage location.
  return saving_locally
             ? PasswordAccountStorageUserState::kSignedInUserSavingLocally
             : PasswordAccountStorageUserState::kSignedInUser;
}

PasswordAccountStorageUsageLevel ComputePasswordAccountStorageUsageLevel(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service) {
  using UserState = PasswordAccountStorageUserState;
  using UsageLevel = PasswordAccountStorageUsageLevel;
  switch (ComputePasswordAccountStorageUserState(pref_service, sync_service)) {
    case UserState::kSignedOutUser:
    case UserState::kSignedOutAccountStoreUser:
    case UserState::kSignedInUser:
    case UserState::kSignedInUserSavingLocally:
      return UsageLevel::kNotUsingAccountStorage;
    case UserState::kSignedInAccountStoreUser:
    case UserState::kSignedInAccountStoreUserSavingLocally:
      return UsageLevel::kUsingAccountStorage;
    case UserState::kSyncUser:
      return UsageLevel::kSyncing;
  }
}

}  // namespace features_util
}  // namespace password_manager
