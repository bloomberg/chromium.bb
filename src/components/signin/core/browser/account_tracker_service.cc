// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/account_tracker_service.h"

#include <stddef.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_pref_names.h"

namespace {

const char kAccountKeyPath[] = "account_id";
const char kAccountEmailPath[] = "email";
const char kAccountGaiaPath[] = "gaia";
const char kAccountHostedDomainPath[] = "hd";
const char kAccountFullNamePath[] = "full_name";
const char kAccountGivenNamePath[] = "given_name";
const char kAccountLocalePath[] = "locale";
const char kAccountPictureURLPath[] = "picture_url";
const char kAccountChildAccountStatusPath[] = "is_child_account";
const char kAdvancedProtectionAccountStatusPath[] =
    "is_under_advanced_protection";

// Account folders used for storing account related data at disk.
const base::FilePath::CharType kAccountsFolder[] =
    FILE_PATH_LITERAL("Accounts");
const base::FilePath::CharType kAvatarImagesFolder[] =
    FILE_PATH_LITERAL("Avatar Images");

// TODO(M48): Remove deprecated preference migration.
const char kAccountServiceFlagsPath[] = "service_flags";

void RemoveDeprecatedServiceFlags(PrefService* pref_service) {
  ListPrefUpdate update(pref_service, AccountTrackerService::kAccountInfoPref);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* dict = nullptr;
    if (update->GetDictionary(i, &dict))
      dict->RemoveWithoutPathExpansion(kAccountServiceFlagsPath, nullptr);
  }
}

// Reads a PNG image from disk and decodes it. If the reading/decoding attempt
// was unsuccessful, an empty image is returned.
gfx::Image ReadImage(const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  if (!base::PathExists(image_path))
    return gfx::Image();
  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    LOG(ERROR) << "Failed to read image from disk: " << image_path;
    return gfx::Image();
  }
  return gfx::Image::CreateFrom1xPNGBytes(
      base::RefCountedString::TakeString(&image_data));
}

// Saves |png_data| to disk at |image_path|.
void SaveImage(scoped_refptr<base::RefCountedMemory> png_data,
               const base::FilePath& image_path) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);
  // Make sure the destination directory exists.
  base::FilePath dir = image_path.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    LOG(ERROR) << "Failed to create parent directory of: " << image_path;
    return;
  }
  if (base::WriteFile(image_path, png_data->front_as<char>(),
                      png_data->size()) == -1) {
    LOG(ERROR) << "Failed to save image to file: " << image_path;
  }
}

// Removes the image at path |image_path|.
void RemoveImage(const base::FilePath& image_path) {
  if (!base::DeleteFile(image_path, false /* recursive */))
    LOG(ERROR) << "Failed to delete image.";
}

}  // namespace

const char AccountTrackerService::kAccountInfoPref[] = "account_info";

const char AccountTrackerService::kChildAccountServiceFlag[] = "uca";

// This must be a string which can never be a valid domain.
const char AccountTrackerService::kNoHostedDomainFound[] = "NO_HOSTED_DOMAIN";

// This must be a string which can never be a valid picture URL.
const char AccountTrackerService::kNoPictureURLFound[] = "NO_PICTURE_URL";

AccountTrackerService::AccountTrackerService() : weak_factory_(this) {}

AccountTrackerService::~AccountTrackerService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void AccountTrackerService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(AccountTrackerService::kAccountInfoPref);
  registry->RegisterIntegerPref(prefs::kAccountIdMigrationState,
                                AccountTrackerService::MIGRATION_NOT_STARTED);
}

void AccountTrackerService::Initialize(PrefService* pref_service,
                                       base::FilePath user_data_dir) {
  DCHECK(pref_service);
  DCHECK(!pref_service_);
  pref_service_ = pref_service;
  LoadFromPrefs();
  user_data_dir_ = std::move(user_data_dir);
  if (!user_data_dir_.empty()) {
    // |image_storage_task_runner_| is a sequenced runner because we want to
    // avoid read and write operations to the same file at the same time.
    image_storage_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    LoadAccountImagesFromDisk();
  }
}

void AccountTrackerService::Shutdown() {
  pref_service_ = nullptr;
  accounts_.clear();
}

void AccountTrackerService::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AccountTrackerService::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::vector<AccountInfo> AccountTrackerService::GetAccounts() const {
  std::vector<AccountInfo> accounts;
  for (const auto& pair : accounts_) {
    accounts.push_back(pair.second.info);
  }
  return accounts;
}

AccountInfo AccountTrackerService::GetAccountInfo(
    const std::string& account_id) const {
  const auto iterator = accounts_.find(account_id);
  if (iterator != accounts_.end())
    return iterator->second.info;

  return AccountInfo();
}

AccountInfo AccountTrackerService::FindAccountInfoByGaiaId(
    const std::string& gaia_id) const {
  if (!gaia_id.empty()) {
    const auto iterator = std::find_if(
        accounts_.begin(), accounts_.end(), [&gaia_id](const auto& pair) {
          return pair.second.info.gaia == gaia_id;
        });
    if (iterator != accounts_.end())
      return iterator->second.info;
  }

  return AccountInfo();
}

AccountInfo AccountTrackerService::FindAccountInfoByEmail(
    const std::string& email) const {
  if (!email.empty()) {
    const auto iterator = std::find_if(
        accounts_.begin(), accounts_.end(), [&email](const auto& pair) {
          return gaia::AreEmailsSame(pair.second.info.email, email);
        });
    if (iterator != accounts_.end())
      return iterator->second.info;
  }

  return AccountInfo();
}

gfx::Image AccountTrackerService::GetAccountImage(
    const std::string& account_id) {
  const auto iterator = accounts_.find(account_id);
  if (iterator != accounts_.end())
    return iterator->second.image;

  return gfx::Image();
}

// static
bool AccountTrackerService::IsMigrationSupported() {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState() const {
  return GetMigrationState(pref_service_);
}

void AccountTrackerService::SetMigrationDone() {
  SetMigrationState(MIGRATION_DONE);
}

void AccountTrackerService::NotifyAccountUpdated(const AccountState& state) {
  DCHECK(!state.info.gaia.empty());
  for (auto& observer : observer_list_)
    observer.OnAccountUpdated(state.info);
}

void AccountTrackerService::NotifyAccountImageUpdated(
    const std::string& account_id,
    const gfx::Image& image) {
  for (auto& observer : observer_list_)
    observer.OnAccountImageUpdated(account_id, image);
}

void AccountTrackerService::NotifyAccountUpdateFailed(
    const std::string& account_id) {
  for (auto& observer : observer_list_)
    observer.OnAccountUpdateFailed(account_id);
}

void AccountTrackerService::NotifyAccountRemoved(const AccountState& state) {
  DCHECK(!state.info.gaia.empty());
  for (auto& observer : observer_list_)
    observer.OnAccountRemoved(state.info);
}

void AccountTrackerService::StartTrackingAccount(
    const std::string& account_id) {
  if (!base::ContainsKey(accounts_, account_id)) {
    DVLOG(1) << "StartTracking " << account_id;
    AccountState state;
    state.info.account_id = account_id;
    state.info.is_child_account = false;
    accounts_.insert(make_pair(account_id, state));
  }
}

void AccountTrackerService::StopTrackingAccount(const std::string& account_id) {
  DVLOG(1) << "StopTracking " << account_id;
  if (base::ContainsKey(accounts_, account_id)) {
    AccountState state = std::move(accounts_[account_id]);
    RemoveFromPrefs(state);
    RemoveAccountImageFromDisk(account_id);
    accounts_.erase(account_id);

    if (!state.info.gaia.empty())
      NotifyAccountRemoved(state);
  }
}

void AccountTrackerService::SetAccountStateFromUserInfo(
    const std::string& account_id,
    const base::DictionaryValue* user_info) {
  DCHECK(base::ContainsKey(accounts_, account_id));
  AccountState& state = accounts_[account_id];

  std::string gaia_id;
  std::string email;
  if (user_info->GetString("id", &gaia_id) &&
      user_info->GetString("email", &email)) {
    state.info.gaia = gaia_id;
    state.info.email = email;

    std::string hosted_domain;
    if (user_info->GetString("hd", &hosted_domain) && !hosted_domain.empty()) {
      state.info.hosted_domain = hosted_domain;
    } else {
      state.info.hosted_domain = kNoHostedDomainFound;
    }

    user_info->GetString("name", &state.info.full_name);
    user_info->GetString("given_name", &state.info.given_name);
    user_info->GetString("locale", &state.info.locale);

    std::string picture_url;
    if (user_info->GetString("picture", &picture_url)) {
      state.info.picture_url = picture_url;
    } else {
      state.info.picture_url = kNoPictureURLFound;
    }
  }
  if (!state.info.gaia.empty())
    NotifyAccountUpdated(state);
  SaveToPrefs(state);
}

void AccountTrackerService::SetAccountImage(const std::string& account_id,
                                            const gfx::Image& image) {
  if (!base::ContainsKey(accounts_, account_id))
    return;
  accounts_[account_id].image = image;
  SaveAccountImageToDisk(account_id, image);
  NotifyAccountImageUpdated(account_id, image);
}

void AccountTrackerService::SetIsChildAccount(const std::string& account_id,
                                              bool is_child_account) {
  DCHECK(base::ContainsKey(accounts_, account_id));
  AccountState& state = accounts_[account_id];
  if (state.info.is_child_account == is_child_account)
    return;
  state.info.is_child_account = is_child_account;
  if (!state.info.gaia.empty())
    NotifyAccountUpdated(state);
  SaveToPrefs(state);
}

void AccountTrackerService::SetIsAdvancedProtectionAccount(
    const std::string& account_id,
    bool is_under_advanced_protection) {
  DCHECK(base::ContainsKey(accounts_, account_id));
  AccountState& state = accounts_[account_id];
  if (state.info.is_under_advanced_protection == is_under_advanced_protection)
    return;
  state.info.is_under_advanced_protection = is_under_advanced_protection;
  if (!state.info.gaia.empty())
    NotifyAccountUpdated(state);
  SaveToPrefs(state);
}

void AccountTrackerService::MigrateToGaiaId() {
  DCHECK_EQ(GetMigrationState(), MIGRATION_IN_PROGRESS);

  std::vector<std::string> to_remove;
  std::vector<AccountState> migrated_accounts;
  for (const auto& pair : accounts_) {
    const std::string& new_account_id = pair.second.info.gaia;
    if (pair.first == new_account_id)
      continue;

    to_remove.push_back(pair.first);

    // If there is already an account keyed to the current account's gaia id,
    // assume this is the result of a partial migration and skip the account
    // that is currently inspected.
    if (base::ContainsKey(accounts_, new_account_id))
      continue;

    AccountState new_state = pair.second;
    new_state.info.account_id = new_account_id;
    SaveToPrefs(new_state);
    migrated_accounts.emplace_back(std::move(new_state));
  }

  // Insert the new migrated accounts.
  for (AccountState& new_state : migrated_accounts) {
    // Copy the AccountState |gaia| member field so that it is not left in
    // an undeterminate state in the structure after std::map::emplace call.
    std::string account_id = new_state.info.gaia;
    SaveToPrefs(new_state);

    accounts_.emplace(std::move(account_id), std::move(new_state));
  }

  // Remove any obsolete account.
  for (const auto& account_id : to_remove) {
    DCHECK(base::ContainsKey(accounts_, account_id));
    AccountState& state = accounts_[account_id];
    RemoveAccountImageFromDisk(account_id);
    RemoveFromPrefs(state);
    accounts_.erase(account_id);
  }
}

bool AccountTrackerService::IsMigrationDone() const {
  if (!IsMigrationSupported())
    return false;

  for (const auto& pair : accounts_) {
    if (pair.first != pair.second.info.gaia)
      return false;
  }

  return true;
}

AccountTrackerService::AccountIdMigrationState
AccountTrackerService::ComputeNewMigrationState() const {
  // If migration is not supported, skip migration.
  if (!IsMigrationSupported())
    return MIGRATION_NOT_STARTED;

  bool migration_required = false;
  for (const auto& pair : accounts_) {
    // If there is any non-migratable account, skip migration.
    if (pair.first.empty() || pair.second.info.gaia.empty())
      return MIGRATION_NOT_STARTED;

    // Migration is required if at least one account is not keyed to its
    // gaia id.
    migration_required |= (pair.first != pair.second.info.gaia);
  }

  return migration_required ? MIGRATION_IN_PROGRESS : MIGRATION_DONE;
}

void AccountTrackerService::SetMigrationState(AccountIdMigrationState state) {
  DCHECK(state != MIGRATION_DONE || IsMigrationDone());
  pref_service_->SetInteger(prefs::kAccountIdMigrationState, state);
}

// static
AccountTrackerService::AccountIdMigrationState
AccountTrackerService::GetMigrationState(const PrefService* pref_service) {
  return static_cast<AccountTrackerService::AccountIdMigrationState>(
      pref_service->GetInteger(prefs::kAccountIdMigrationState));
}

base::FilePath AccountTrackerService::GetImagePathFor(
    const std::string& account_id) {
  return user_data_dir_.Append(kAccountsFolder)
      .Append(kAvatarImagesFolder)
      .AppendASCII(account_id);
}

void AccountTrackerService::OnAccountImageLoaded(const std::string& account_id,
                                                 gfx::Image image) {
  if (base::ContainsKey(accounts_, account_id) &&
      accounts_[account_id].image.IsEmpty()) {
    accounts_[account_id].image = image;
    NotifyAccountImageUpdated(account_id, image);
  }
}

void AccountTrackerService::LoadAccountImagesFromDisk() {
  if (!image_storage_task_runner_)
    return;
  for (const auto& pair : accounts_) {
    const std::string& account_id = pair.second.info.account_id;
    PostTaskAndReplyWithResult(
        image_storage_task_runner_.get(), FROM_HERE,
        base::BindOnce(&ReadImage, GetImagePathFor(account_id)),
        base::BindOnce(&AccountTrackerService::OnAccountImageLoaded,
                       weak_factory_.GetWeakPtr(), account_id));
  }
}

void AccountTrackerService::SaveAccountImageToDisk(
    const std::string& account_id,
    const gfx::Image& image) {
  if (!image_storage_task_runner_)
    return;
  image_storage_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&SaveImage, image.As1xPNGBytes(),
                                GetImagePathFor(account_id)));
}

void AccountTrackerService::RemoveAccountImageFromDisk(
    const std::string& account_id) {
  if (!image_storage_task_runner_)
    return;
  image_storage_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoveImage, GetImagePathFor(account_id)));
}

void AccountTrackerService::LoadFromPrefs() {
  const base::ListValue* list = pref_service_->GetList(kAccountInfoPref);
  std::set<std::string> to_remove;
  bool contains_deprecated_service_flags = false;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* dict;
    if (list->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value)) {
        std::string account_id = base::UTF16ToUTF8(value);

        // Ignore incorrectly persisted non-canonical account ids.
        if (account_id.find('@') != std::string::npos &&
            account_id != gaia::CanonicalizeEmail(account_id)) {
          to_remove.insert(account_id);
          continue;
        }

        StartTrackingAccount(account_id);
        AccountState& state = accounts_[account_id];

        if (dict->GetString(kAccountGaiaPath, &value))
          state.info.gaia = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountEmailPath, &value))
          state.info.email = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountHostedDomainPath, &value))
          state.info.hosted_domain = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountFullNamePath, &value))
          state.info.full_name = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountGivenNamePath, &value))
          state.info.given_name = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountLocalePath, &value))
          state.info.locale = base::UTF16ToUTF8(value);
        if (dict->GetString(kAccountPictureURLPath, &value))
          state.info.picture_url = base::UTF16ToUTF8(value);

        bool is_child_account = false;
        // Migrate deprecated service flag preference.
        const base::ListValue* service_flags_list;
        if (dict->GetList(kAccountServiceFlagsPath, &service_flags_list)) {
          contains_deprecated_service_flags = true;
          std::string flag_string;
          for (const auto& flag : *service_flags_list) {
            if (flag.GetAsString(&flag_string) &&
                flag_string == kChildAccountServiceFlag) {
              is_child_account = true;
              break;
            }
          }
          state.info.is_child_account = is_child_account;
        }
        if (dict->GetBoolean(kAccountChildAccountStatusPath, &is_child_account))
          state.info.is_child_account = is_child_account;

        bool is_under_advanced_protection = false;
        if (dict->GetBoolean(kAdvancedProtectionAccountStatusPath,
                             &is_under_advanced_protection)) {
          state.info.is_under_advanced_protection =
              is_under_advanced_protection;
        }

        if (!state.info.gaia.empty())
          NotifyAccountUpdated(state);
      }
    }
  }

  UMA_HISTOGRAM_BOOLEAN("Signin.AccountTracker.DeprecatedServiceFlagDeleted",
                        contains_deprecated_service_flags);

  if (contains_deprecated_service_flags)
    RemoveDeprecatedServiceFlags(pref_service_);

  // Remove any obsolete prefs.
  for (auto account_id : to_remove) {
    AccountState state;
    state.info.account_id = account_id;
    RemoveFromPrefs(state);
    RemoveAccountImageFromDisk(account_id);
  }

  if (IsMigrationSupported()) {
    if (GetMigrationState() != MIGRATION_DONE) {
      const AccountIdMigrationState new_state = ComputeNewMigrationState();
      SetMigrationState(new_state);

      if (new_state == MIGRATION_IN_PROGRESS) {
        MigrateToGaiaId();
      }
    }
  } else {
    // ChromeOS running on Linux and Linux share the preferences, so the
    // migration may have been performed on Linux. Reset the migration
    // state to ensure that the same code path is used whether ChromeOS
    // is running on Linux on a dev build or on real ChromeOS device.
    SetMigrationState(MIGRATION_NOT_STARTED);
  }

  DCHECK(GetMigrationState() != MIGRATION_DONE || IsMigrationDone());
  UMA_HISTOGRAM_ENUMERATION("Signin.AccountTracker.GaiaIdMigrationState",
                            GetMigrationState(), NUM_MIGRATION_STATES);

  UMA_HISTOGRAM_COUNTS_100("Signin.AccountTracker.CountOfLoadedAccounts",
                           accounts_.size());
}

void AccountTrackerService::SaveToPrefs(const AccountState& state) {
  if (!pref_service_)
    return;

  base::DictionaryValue* dict = nullptr;
  base::string16 account_id_16 = base::UTF8ToUTF16(state.info.account_id);
  ListPrefUpdate update(pref_service_, kAccountInfoPref);
  for (size_t i = 0; i < update->GetSize(); ++i, dict = nullptr) {
    if (update->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value) && value == account_id_16)
        break;
    }
  }

  if (!dict) {
    dict = new base::DictionaryValue();
    update->Append(base::WrapUnique(dict));
    // |dict| is invalidated at this point, so it needs to be reset.
    update->GetDictionary(update->GetSize() - 1, &dict);
    dict->SetString(kAccountKeyPath, account_id_16);
  }

  dict->SetString(kAccountEmailPath, state.info.email);
  dict->SetString(kAccountGaiaPath, state.info.gaia);
  dict->SetString(kAccountHostedDomainPath, state.info.hosted_domain);
  dict->SetString(kAccountFullNamePath, state.info.full_name);
  dict->SetString(kAccountGivenNamePath, state.info.given_name);
  dict->SetString(kAccountLocalePath, state.info.locale);
  dict->SetString(kAccountPictureURLPath, state.info.picture_url);
  dict->SetBoolean(kAccountChildAccountStatusPath, state.info.is_child_account);
  dict->SetBoolean(kAdvancedProtectionAccountStatusPath,
                   state.info.is_under_advanced_protection);
}

void AccountTrackerService::RemoveFromPrefs(const AccountState& state) {
  if (!pref_service_)
    return;

  base::string16 account_id_16 = base::UTF8ToUTF16(state.info.account_id);
  ListPrefUpdate update(pref_service_, kAccountInfoPref);
  for (size_t i = 0; i < update->GetSize(); ++i) {
    base::DictionaryValue* dict = nullptr;
    if (update->GetDictionary(i, &dict)) {
      base::string16 value;
      if (dict->GetString(kAccountKeyPath, &value) && value == account_id_16) {
        update->Remove(i, nullptr);
        break;
      }
    }
  }
}

std::string AccountTrackerService::PickAccountIdForAccount(
    const std::string& gaia,
    const std::string& email) const {
  return PickAccountIdForAccount(pref_service_, gaia, email);
}

// static
std::string AccountTrackerService::PickAccountIdForAccount(
    const PrefService* pref_service,
    const std::string& gaia,
    const std::string& email) {
  DCHECK(!gaia.empty() ||
         GetMigrationState(pref_service) == MIGRATION_NOT_STARTED);
  DCHECK(!email.empty());
  switch (GetMigrationState(pref_service)) {
    case MIGRATION_NOT_STARTED:
      // Some tests don't use a real email address.  To support these cases,
      // don't try to canonicalize these strings.
      return (email.find('@') == std::string::npos)
                 ? email
                 : gaia::CanonicalizeEmail(email);
    case MIGRATION_IN_PROGRESS:
    case MIGRATION_DONE:
      return gaia;
    default:
      NOTREACHED();
      return email;
  }
}

std::string AccountTrackerService::SeedAccountInfo(const std::string& gaia,
                                                   const std::string& email) {
  const std::string account_id = PickAccountIdForAccount(gaia, email);
  const bool already_exists = base::ContainsKey(accounts_, account_id);
  StartTrackingAccount(account_id);
  AccountState& state = accounts_[account_id];
  DCHECK(!already_exists || state.info.gaia.empty() || state.info.gaia == gaia);
  state.info.gaia = gaia;
  state.info.email = email;
  SaveToPrefs(state);

  DVLOG(1) << "AccountTrackerService::SeedAccountInfo"
           << " account_id=" << account_id << " gaia_id=" << gaia
           << " email=" << email;

  return account_id;
}

std::string AccountTrackerService::SeedAccountInfo(AccountInfo info) {
  info.account_id = PickAccountIdForAccount(info.gaia, info.email);

  if (!base::ContainsKey(accounts_, info.account_id)) {
    StartTrackingAccount(info.account_id);
  }

  AccountState& state = accounts_[info.account_id];
  // Update the missing fields in |state.info| with |info|.
  if (state.info.UpdateWith(info)) {
    if (!state.info.gaia.empty())
      NotifyAccountUpdated(state);

    SaveToPrefs(state);
  }
  return info.account_id;
}

void AccountTrackerService::RemoveAccount(const std::string& account_id) {
  StopTrackingAccount(account_id);
}
