// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/containers/cxx20_erase.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/account_manager/add_account_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "components/account_manager_core/account.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"

namespace {

void DeleteProfile(const base::FilePath& profile_path,
                   ProfileMetrics::ProfileDelete delete_metric) {
  // Pass an empty callback because this should never delete the last profile.
  // TODO(https://crbug.com/1257610): ensure that the user cannot cancel the
  // profile deletion.
  g_browser_process->profile_manager()->MaybeScheduleProfileForDeletion(
      profile_path, base::DoNothing(), delete_metric);
}

}  // namespace

AccountProfileMapper::AccountProfileMapper(
    account_manager::AccountManagerFacade* facade,
    ProfileAttributesStorage* storage,
    PrefService* local_state)
    : account_manager_facade_(facade),
      profile_attributes_storage_(storage),
      account_cache_(local_state) {
  DCHECK(profile_attributes_storage_);

  // This must be done before OnGetAccountsCompleted() is called, to avoid
  // unnecessary profile deletion.
  MigrateOldProfiles();

  account_manager_facade_observation_.Observe(account_manager_facade_);
  profile_attributes_storage_observation_.Observe(profile_attributes_storage_);
  account_manager_facade_->GetAccounts(
      base::BindOnce(&AccountProfileMapper::OnGetAccountsCompleted,
                     weak_factory_.GetWeakPtr()));
}

AccountProfileMapper::~AccountProfileMapper() = default;

void AccountProfileMapper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AccountProfileMapper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AccountProfileMapper::GetAccounts(const base::FilePath& profile_path,
                                       ListAccountsCallback callback) {
  if (!initialized_) {
    initialization_callbacks_.push_back(base::BindOnce(
        &AccountProfileMapper::GetAccounts, weak_factory_.GetWeakPtr(),
        profile_path, std::move(callback)));
    return;
  }
  std::vector<account_manager::Account> accounts;
  const ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path);
  // `entry` may be null during profile deletion, as it is destroyed before the
  // profile.
  if (entry) {
    for (const std::string& gaia_id : entry->GetGaiaIds()) {
      const account_manager::Account* account =
          account_cache_.FindAccountByGaiaId(gaia_id);
      if (!account) {
        NOTREACHED() << "Account " << gaia_id << " missing.";
        continue;
      }
      accounts.push_back(*account);
    }
  }
  std::move(callback).Run(accounts);
}

void AccountProfileMapper::GetPersistentErrorForAccount(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account,
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback) {
  if (!initialized_) {
    initialization_callbacks_.push_back(
        base::BindOnce(&AccountProfileMapper::GetPersistentErrorForAccount,
                       weak_factory_.GetWeakPtr(), profile_path, account,
                       std::move(callback)));
    return;
  }
  if (!ProfileContainsAccount(profile_path, account)) {
    std::move(callback).Run(
        GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP));
    return;
  }
  account_manager_facade_->GetPersistentErrorForAccount(account,
                                                        std::move(callback));
}

std::unique_ptr<OAuth2AccessTokenFetcher>
AccountProfileMapper::CreateAccessTokenFetcher(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account,
    const std::string& oauth_consumer_name,
    OAuth2AccessTokenConsumer* consumer) {
  // TODO(https://crbug.com/1226045): Create a fetcher that can wait on
  // initialization of the class.
  if (!ProfileContainsAccount(profile_path, account)) {
    return std::make_unique<OAuth2AccessTokenFetcherImmediateError>(
        consumer,
        GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP));
  }

  return account_manager_facade_->CreateAccessTokenFetcher(
      account, oauth_consumer_name, consumer);
}

void AccountProfileMapper::GetAccountsMap(MapAccountsCallback callback) {
  if (!initialized_) {
    initialization_callbacks_.push_back(
        base::BindOnce(&AccountProfileMapper::GetAccountsMap,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  std::map<base::FilePath, std::vector<account_manager::Account>> accounts_map;
  AccountCache::AccountByGaiaIdMap unassigned_accounts =
      account_cache_.GetAccountsCopy();
  for (ProfileAttributesEntry* entry :
       profile_attributes_storage_->GetAllProfilesAttributes()) {
    const base::FilePath path = entry->GetPath();
    for (const std::string& gaia_id : entry->GetGaiaIds()) {
      const account_manager::Account* account =
          account_cache_.FindAccountByGaiaId(gaia_id);
      if (!account) {
        NOTREACHED() << "Account " << gaia_id << " missing.";
        continue;
      }
      accounts_map[path].push_back(*account);
      unassigned_accounts.erase(gaia_id);
    }
  }
  for (const auto& pair : unassigned_accounts)
    accounts_map[base::FilePath()].push_back(pair.second);
  std::move(callback).Run(accounts_map);
}

void AccountProfileMapper::ShowAddAccountDialog(
    const base::FilePath& profile_path,
    account_manager::AccountManagerFacade::AccountAdditionSource source,
    AddAccountCallback callback) {
  DCHECK(!profile_path.empty())
      << "For a new profile use ShowAddAccountDialogAndCreateNewProfile()";
  AddAccountInternal(profile_path, source, std::move(callback));
}

void AccountProfileMapper::AddAccount(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account,
    AddAccountCallback callback) {
  DCHECK(!profile_path.empty())
      << "For a new profile use CreateNewProfileWithAccount()";
  AddAccountInternal(profile_path, account, std::move(callback));
}

void AccountProfileMapper::ShowAddAccountDialogAndCreateNewProfile(
    account_manager::AccountManagerFacade::AccountAdditionSource source,
    AddAccountCallback callback) {
  AddAccountInternal(base::FilePath(), source, std::move(callback));
}

void AccountProfileMapper::CreateNewProfileWithAccount(
    const account_manager::AccountKey& account,
    AddAccountCallback callback) {
  AddAccountInternal(base::FilePath(), account, std::move(callback));
}

void AccountProfileMapper::OnAccountUpserted(
    const account_manager::Account& account) {
  if (account.key.account_type() != account_manager::AccountType::kGaia)
    return;

  if (!initialized_) {
    initialization_callbacks_.push_back(
        base::BindOnce(&AccountProfileMapper::OnAccountUpserted,
                       weak_factory_.GetWeakPtr(), account));
    return;
  }

  if (account_cache_.FindAccountByGaiaId(account.key.id())) {
    // The account is already known. This is an account update. Propagate the
    // update to all profiles that have this account.
    std::vector<ProfileAttributesEntry*> entries =
        profile_attributes_storage_->GetAllProfilesAttributes();
    std::vector<base::FilePath> profiles_with_updated_account;
    for (const ProfileAttributesEntry* entry : entries) {
      if (entry->GetGaiaIds().contains(account.key.id()))
        profiles_with_updated_account.push_back(entry->GetPath());
    }

    // Send a notification with empty path if `account` is unassigned.
    if (profiles_with_updated_account.empty())
      profiles_with_updated_account.push_back(base::FilePath());

    // TODO(https://crbug.com/1262946): Update this code when
    // OnAccountUpserted() takes multiple paths.
    for (const base::FilePath& profile_path : profiles_with_updated_account) {
      for (auto& obs : observers_)
        obs.OnAccountUpserted(profile_path, account);
    }

    // Do not return early to update the list of accounts below.
    // `account_cache_` might be stale here. There is a small chance that
    // `account` has been already removed and re-added and that we took re-add
    // operation for an update.
  }

  account_manager_facade_->GetAccounts(
      base::BindOnce(&AccountProfileMapper::OnGetAccountsCompleted,
                     weak_factory_.GetWeakPtr()));
}

void AccountProfileMapper::OnAccountRemoved(
    const account_manager::Account& account) {
  if (account.key.account_type() != account_manager::AccountType::kGaia)
    return;

  account_manager_facade_->GetAccounts(
      base::BindOnce(&AccountProfileMapper::OnGetAccountsCompleted,
                     weak_factory_.GetWeakPtr()));
}

void AccountProfileMapper::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry_to_be_removed =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path);
  DCHECK(entry_to_be_removed);

  // Compute a set of accounts that are assigned to at least one profile.
  base::flat_set<std::string> assigned_account_ids;
  for (ProfileAttributesEntry* entry :
       profile_attributes_storage_->GetAllProfilesAttributes()) {
    if (entry == entry_to_be_removed)
      continue;
    base::flat_set<std::string> profile_account_ids = entry->GetGaiaIds();
    assigned_account_ids.insert(profile_account_ids.begin(),
                                profile_account_ids.end());
  }

  // Compute a list of accounts that will become unassigned after a profile at
  // `profile_path` is removed.
  base::flat_set<std::string> freed_account_ids =
      entry_to_be_removed->GetGaiaIds();
  std::vector<std::string> unassigned_account_ids;
  base::ranges::set_difference(freed_account_ids, assigned_account_ids,
                               std::back_inserter(unassigned_account_ids));

  // Notify observers about accounts that became unassiged.
  for (const std::string& unassiged_account_id : unassigned_account_ids) {
    const account_manager::Account* account =
        account_cache_.FindAccountByGaiaId(unassiged_account_id);
    // `account_cache_` might be outdated.
    if (!account)
      continue;

    for (auto& obs : observers_)
      obs.OnAccountUpserted(base::FilePath(), *account);
  }
}

void AccountProfileMapper::UpsertAccountForTesting(
    const base::FilePath& profile_path,
    const account_manager::Account& account,
    const std::string& token_value) {
  if (!initialized_) {
    initialization_callbacks_.push_back(base::BindOnce(
        &AccountProfileMapper::UpsertAccountForTesting,
        weak_factory_.GetWeakPtr(), profile_path, account, token_value));
    return;
  }

  add_account_helpers_.push_back(std::make_unique<AddAccountHelper>(
      base::BindRepeating(
          &AccountProfileMapper::IsAccountInCache,
          // `this` owns the helper, so base::Unretained() is fine.
          base::Unretained(this)),
      account_manager_facade_, profile_attributes_storage_));
  AddAccountHelper* helper = add_account_helpers_.back().get();

  helper->UpsertAccountForTesting(  // IN-TEST
      profile_path, account, token_value,
      base::BindOnce(&AccountProfileMapper::OnAddAccountCompleted,
                     weak_factory_.GetWeakPtr(), helper, base::DoNothing()));
}

void AccountProfileMapper::RemoveAccountForTesting(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account_key) {
  if (!initialized_) {
    initialization_callbacks_.push_back(
        base::BindOnce(&AccountProfileMapper::RemoveAccountForTesting,
                       weak_factory_.GetWeakPtr(), profile_path, account_key));
    return;
  }
  account_manager_facade_->RemoveAccountForTesting(account_key);  // IN-TEST
}

bool AccountProfileMapper::IsAccountInCache(
    const account_manager::Account& account) {
  return account.key.account_type() == account_manager::AccountType::kGaia &&
         account_cache_.FindAccountByGaiaId(account.key.id());
}

void AccountProfileMapper::AddAccountInternal(
    const base::FilePath& profile_path,
    const absl::variant<
        account_manager::AccountManagerFacade::AccountAdditionSource,
        account_manager::AccountKey>& source_or_accountkey,
    AddAccountCallback callback) {
  if (!initialized_) {
    initialization_callbacks_.push_back(base::BindOnce(
        &AccountProfileMapper::AddAccountInternal, weak_factory_.GetWeakPtr(),
        profile_path, source_or_accountkey, std::move(callback)));
    return;
  }

  absl::variant<account_manager::AccountManagerFacade::AccountAdditionSource,
                account_manager::Account>
      source_or_account;
  // If an account is provided, check that it exists in the facade.
  if (const account_manager::AccountKey* account_key =
          absl::get_if<account_manager::AccountKey>(&source_or_accountkey)) {
    if (account_key->account_type() != account_manager::AccountType::kGaia) {
      if (callback)
        std::move(callback).Run(absl::nullopt);
      return;
    }
    const account_manager::Account* account =
        account_cache_.FindAccountByGaiaId(account_key->id());
    if (!account) {
      if (callback)
        std::move(callback).Run(absl::nullopt);
      return;
    } else {
      source_or_account = *account;
    }
  } else {
    source_or_account =
        absl::get<account_manager::AccountManagerFacade::AccountAdditionSource>(
            source_or_accountkey);
  }

  add_account_helpers_.push_back(std::make_unique<AddAccountHelper>(
      base::BindRepeating(
          &AccountProfileMapper::IsAccountInCache,
          // `this` owns the helper, so base::Unretained() is fine.
          base::Unretained(this)),
      account_manager_facade_, profile_attributes_storage_));
  AddAccountHelper* helper = add_account_helpers_.back().get();
  helper->Start(
      profile_path, source_or_account,
      base::BindOnce(&AccountProfileMapper::OnAddAccountCompleted,
                     weak_factory_.GetWeakPtr(), helper, std::move(callback)));
}

void AccountProfileMapper::OnAddAccountCompleted(
    AddAccountHelper* helper,
    AddAccountCallback callback,
    const absl::optional<AddAccountResult>& result) {
  // Note: the new account may or may not be in `account_cache_`. There is a
  // small possibility that an account was already removed from the OS. As a
  // result, this function does not use `account_cache_` at all.
  // Exclude unassigned accounts because `OnGetAccountsCompleted()` will notify
  // observers about them.
  if (result && !result->profile_path.empty()) {
    for (auto& obs : observers_)
      obs.OnAccountUpserted(result->profile_path, result->account);
  }

  if (callback)
    std::move(callback).Run(result);

  size_t erased_count =
      base::EraseIf(add_account_helpers_, base::MatchesUniquePtr(helper));
  DCHECK_EQ(erased_count, 1u);
  if (add_account_helpers_.empty()) {
    account_manager_facade_->GetAccounts(
        base::BindOnce(&AccountProfileMapper::OnGetAccountsCompleted,
                       weak_factory_.GetWeakPtr()));
  }
}

std::vector<std::pair<base::FilePath, std::string>>
AccountProfileMapper::RemoveStaleAccounts() {
  std::vector<std::pair<base::FilePath, std::string>> removed_ids;
  std::vector<ProfileAttributesEntry*> entries =
      profile_attributes_storage_->GetAllProfilesAttributes();
  // For each profile.
  for (ProfileAttributesEntry* entry : entries) {
    base::flat_set<std::string> entry_ids = entry->GetGaiaIds();
    bool entry_needs_update = false;
    // For each account in the profile.
    auto it = entry_ids.begin();
    while (it != entry_ids.end()) {
      if (account_cache_.FindAccountByGaiaId(*it)) {
        ++it;
      } else {
        // An account in the profile is no longer in the system.
        entry_needs_update = true;
        removed_ids.push_back(std::make_pair(entry->GetPath(), *it));
        it = entry_ids.erase(it);
      }
    }
    if (entry_needs_update)
      entry->SetGaiaIds(entry_ids);
    if (ShouldDeleteProfile(entry)) {
      DeleteProfile(
          entry->GetPath(),
          ProfileMetrics::DELETE_PROFILE_PRIMARY_ACCOUNT_REMOVED_LACROS);
    }
  }
  return removed_ids;
}

std::vector<const account_manager::Account*>
AccountProfileMapper::AddNewGaiaAccounts(
    const std::vector<account_manager::Account>& system_accounts,
    AccountCache::AccountIdSet lacros_account_ids,
    ProfileAttributesEntry* entry_for_new_accounts) {
  // Add the set of Gaia IDs in the profiles to `lacros_account_ids`. This is
  // important because accounts created by Chrome are initially added to
  // ProfileAttributesStorage first, but not to account cache. Otherwise, we'd
  // treat these new accounts as new unassigned accounts.
  std::vector<ProfileAttributesEntry*> entries =
      profile_attributes_storage_->GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::flat_set<std::string> entry_ids = entry->GetGaiaIds();
    lacros_account_ids.insert(entry_ids.begin(), entry_ids.end());
  }

  // Diff computed set against system accounts.
  std::vector<const account_manager::Account*> added_accounts;
  for (const account_manager::Account& account : system_accounts) {
    if (account.key.account_type() == account_manager::AccountType::kGaia &&
        !lacros_account_ids.contains(account.key.id())) {
      added_accounts.push_back(&account);
    }
  }
  // Update the `ProfileAttributesEntry`.
  if (entry_for_new_accounts) {
    base::flat_set<std::string> gaia_ids = entry_for_new_accounts->GetGaiaIds();
    for (const account_manager::Account* account : added_accounts)
      gaia_ids.insert(account->key.id());
    entry_for_new_accounts->SetGaiaIds(gaia_ids);
  }
  return added_accounts;
}

void AccountProfileMapper::OnGetAccountsCompleted(
    const std::vector<account_manager::Account>& system_accounts) {
  account_cache_.UpdateAccounts(system_accounts);

  // `AccountManagerFacade` may call `OnAccountUpserted()` before the
  // `ShowAddAccountDialog()` callback, which may result in this function being
  // called during the account addition. In this case, notify helpers and
  // return immediately now, to avoid incorrectly assigning accounts to the main
  // profile. Another call will be triggered at the end of the account addition
  // to sort things up.
  if (!add_account_helpers_.empty()) {
    // Create a local copy of `add_account_helpers_` because
    // `OnAccountCacheUpdated()` may delete a helper in the middle of the
    // iteration.
    std::vector<AddAccountHelper*> local_add_account_helpers;
    local_add_account_helpers.reserve(add_account_helpers_.size());
    for (const auto& add_account_helper : add_account_helpers_)
      local_add_account_helpers.push_back(add_account_helper.get());

    for (auto* add_account_helper : local_add_account_helpers)
      add_account_helper->OnAccountCacheUpdated();
    return;
  }

  // Accounts that were removed.
  std::vector<std::pair<base::FilePath, std::string>> removed_ids =
      RemoveStaleAccounts();
  // Accounts that were added.
  ProfileAttributesEntry* entry_for_new_accounts =
      MaybeGetProfileForNewAccounts();

  if (initialized_) {
    DCHECK(initialization_callbacks_.empty());

    AccountCache::AccountByGaiaIdMap old_cache =
        account_cache_.UpdateSnapshot();
    AccountCache::AccountIdSet old_account_ids = base::MakeFlatSet<std::string>(
        old_cache, {},
        [](const auto& mapped_pair) { return mapped_pair.first; });
    std::vector<const account_manager::Account*> added_accounts =
        AddNewGaiaAccounts(system_accounts, std::move(old_account_ids),
                           entry_for_new_accounts);

    // Call observers once all entries are updated.
    base::FilePath path_for_new_accounts;
    if (entry_for_new_accounts)
      path_for_new_accounts = entry_for_new_accounts->GetPath();
    for (auto& obs : observers_) {
      for (const auto* account : added_accounts) {
        DCHECK_EQ(account->key.account_type(),
                  account_manager::AccountType::kGaia);
        obs.OnAccountUpserted(path_for_new_accounts, *account);
      }
      for (const auto& pair : removed_ids) {
        auto it = old_cache.find(pair.second);
        if (it == old_cache.cend()) {
          NOTREACHED() << "Account " << pair.second << " missing.";
          continue;
        }
        obs.OnAccountRemoved(pair.first, it->second);
      }
    }
  } else {
    AccountCache::AccountIdSet old_account_ids =
        account_cache_.CreateSnapshot();
    AddNewGaiaAccounts(system_accounts, std::move(old_account_ids),
                       entry_for_new_accounts);
    initialized_ = true;
    for (auto& callback : initialization_callbacks_)
      std::move(callback).Run();
    initialization_callbacks_.clear();
  }
}

bool AccountProfileMapper::ProfileContainsAccount(
    const base::FilePath& profile_path,
    const account_manager::AccountKey& account) const {
  const ProfileAttributesEntry* entry =
      profile_attributes_storage_->GetProfileAttributesWithPath(profile_path);
  if (!entry)
    return false;
  return entry->GetGaiaIds().contains(account.id());
}

ProfileAttributesEntry* AccountProfileMapper::MaybeGetProfileForNewAccounts()
    const {
  std::vector<ProfileAttributesEntry*> entries =
      profile_attributes_storage_->GetAllProfilesAttributes();
  // Ignore omitted profiles.
  base::EraseIf(entries,
                [](const auto* entry) -> bool { return entry->IsOmitted(); });
  if (entries.empty())
    return nullptr;  // Happens in tests.
  // If there are multiple profiles, leave new accounts unassigned.
  if (entries.size() > 1)
    return nullptr;
  // Otherwise auto-assign new accounts to the main profile.
  DCHECK(Profile::IsMainProfilePath(entries[0]->GetPath()));
  return entries[0];
}

bool AccountProfileMapper::ShouldDeleteProfile(
    ProfileAttributesEntry* entry) const {
  // Delete profile if its primary account has been removed.
  const std::string& primary_gaia_id = entry->GetGAIAId();
  bool primary_account_deleted =
      !primary_gaia_id.empty() &&
      !account_cache_.FindAccountByGaiaId(primary_gaia_id);

  if (Profile::IsMainProfilePath(entry->GetPath())) {
    // Never delete the main profile.
    if (primary_account_deleted) {
      // Primary account of the main profile must never be deleted. A CHECK here
      // can possibly put a device in a crash loop, so upload a crash report
      // silently instead.
      DLOG(ERROR) << "Primary account has been removed from the main profile";
      base::debug::DumpWithoutCrashing();
    }
    return false;
  }

  return primary_account_deleted;
}

void AccountProfileMapper::MigrateOldProfiles() {
  for (ProfileAttributesEntry* entry :
       profile_attributes_storage_->GetAllProfilesAttributes()) {
    // Populate missing Gaia Ids.
    std::string primary_gaia_id = entry->GetGAIAId();
    if (!primary_gaia_id.empty()) {
      base::flat_set<std::string> gaia_ids = entry->GetGaiaIds();
      auto inserted_result = gaia_ids.insert(primary_gaia_id);
      if (inserted_result.second)
        entry->SetGaiaIds(gaia_ids);
    }

    // Delete non-syncing profiles.
    // TODO(https://crbug.com/1260291): Revisit this once non-syncing profiles
    // are allowed.
    base::FilePath profile_path = entry->GetPath();
    if (!entry->IsAuthenticated() &&
        !Profile::IsMainProfilePath(profile_path)) {
      DeleteProfile(
          profile_path,
          ProfileMetrics::DELETE_PROFILE_SIGNIN_REQUIRED_MIRROR_LACROS);
    }
  }
}
