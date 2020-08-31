// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/users/supervised_user_manager_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/supervised/supervised_user_authentication.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_util.h"

using content::BrowserThread;

namespace {

// Names for pref keys in Local State.
// A map from supervised user local user id to sync user id.
const char kSupervisedUserSyncId[] = "ManagedUserSyncId";

// A map from supervised user id to manager user id.
const char kSupervisedUserManagers[] = "ManagedUserManagers";

// A map from supervised user id to manager display name.
const char kSupervisedUserManagerNames[] = "ManagedUserManagerNames";

// A map from supervised user id to manager display e-mail.
const char kSupervisedUserManagerDisplayEmails[] =
    "ManagedUserManagerDisplayEmails";

// A vector pref of the supervised accounts defined on this device, that had
// not logged in yet.
const char kSupervisedUsersFirstRun[] = "LocallyManagedUsersFirstRun";

const char kSupervisedUserCreationTransactionDisplayName[] =
    "LocallyManagedUserCreationTransactionDisplayName";

const char kSupervisedUserCreationTransactionUserId[] =
    "LocallyManagedUserCreationTransactionUserId";

// A map from user id to password schema id.
const char kSupervisedUserPasswordSchema[] = "SupervisedUserPasswordSchema";

// A map from user id to password salt.
const char kSupervisedUserPasswordSalt[] = "SupervisedUserPasswordSalt";

// A map from user id to password revision.
const char kSupervisedUserPasswordRevision[] = "SupervisedUserPasswordRevision";

// A map from user id to flag indicating if password should be updated upon
// signin.
const char kSupervisedUserNeedPasswordUpdate[] =
    "SupervisedUserNeedPasswordUpdate";

// A map from user id to flag indicating if cryptohome does not have signature
// key.
const char kSupervisedUserIncompleteKey[] = "SupervisedUserHasIncompleteKey";

}  // namespace

namespace chromeos {

const char kSchemaVersion[] = "SchemaVersion";
const char kPasswordRevision[] = "PasswordRevision";
const char kSalt[] = "PasswordSalt";
const char kPasswordSignature[] = "PasswordSignature";
const char kEncryptedPassword[] = "EncryptedPassword";
const char kRequirePasswordUpdate[] = "RequirePasswordUpdate";
const char kHasIncompleteKey[] = "HasIncompleteKey";
const char kPasswordEncryptionKey[] = "password.hmac.encryption";
const char kPasswordSignatureKey[] = "password.hmac.signature";

const char kPasswordUpdateFile[] = "password.update";
const int kMinPasswordRevision = 1;

// static
void SupervisedUserManager::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(kSupervisedUsersFirstRun);
  registry->RegisterStringPref(kSupervisedUserCreationTransactionDisplayName,
                               "");
  registry->RegisterStringPref(kSupervisedUserCreationTransactionUserId, "");
  registry->RegisterDictionaryPref(kSupervisedUserSyncId);
  registry->RegisterDictionaryPref(kSupervisedUserManagers);
  registry->RegisterDictionaryPref(kSupervisedUserManagerNames);
  registry->RegisterDictionaryPref(kSupervisedUserManagerDisplayEmails);

  registry->RegisterDictionaryPref(kSupervisedUserPasswordSchema);
  registry->RegisterDictionaryPref(kSupervisedUserPasswordSalt);
  registry->RegisterDictionaryPref(kSupervisedUserPasswordRevision);

  registry->RegisterDictionaryPref(kSupervisedUserNeedPasswordUpdate);
  registry->RegisterDictionaryPref(kSupervisedUserIncompleteKey);
}

SupervisedUserManagerImpl::SupervisedUserManagerImpl(
    ChromeUserManagerImpl* owner)
    : owner_(owner), cros_settings_(CrosSettings::Get()) {
  // SupervisedUserManager instance should be used only on UI thread.
  // (or in unit_tests)
  if (base::ThreadTaskRunnerHandle::IsSet())
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  authentication_.reset(new SupervisedUserAuthentication(this));
}

SupervisedUserManagerImpl::~SupervisedUserManagerImpl() {}

bool SupervisedUserManagerImpl::HasSupervisedUsers(
    const std::string& manager_id) const {
  const user_manager::UserList& users = owner_->GetUsers();
  for (user_manager::UserList::const_iterator it = users.begin();
       it != users.end(); ++it) {
    if ((*it)->GetType() == user_manager::USER_TYPE_SUPERVISED) {
      if (manager_id == GetManagerUserId((*it)->GetAccountId().GetUserEmail()))
        return true;
    }
  }
  return false;
}

std::string SupervisedUserManagerImpl::GetUserSyncId(
    const std::string& user_id) const {
  std::string result;
  GetUserStringValue(user_id, kSupervisedUserSyncId, &result);
  return result;
}

base::string16 SupervisedUserManagerImpl::GetManagerDisplayName(
    const std::string& user_id) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* manager_names =
      local_state->GetDictionary(kSupervisedUserManagerNames);
  base::string16 result;
  if (manager_names->GetStringWithoutPathExpansion(user_id, &result) &&
      !result.empty())
    return result;
  return base::UTF8ToUTF16(GetManagerDisplayEmail(user_id));
}

std::string SupervisedUserManagerImpl::GetManagerUserId(
    const std::string& user_id) const {
  std::string result;
  GetUserStringValue(user_id, kSupervisedUserManagers, &result);
  return result;
}

std::string SupervisedUserManagerImpl::GetManagerDisplayEmail(
    const std::string& user_id) const {
  std::string result;
  if (GetUserStringValue(user_id, kSupervisedUserManagerDisplayEmails,
                         &result) &&
      !result.empty())
    return result;
  return GetManagerUserId(user_id);
}

void SupervisedUserManagerImpl::GetPasswordInformation(
    const std::string& user_id,
    base::DictionaryValue* result) {
  int value;
  if (GetUserIntegerValue(user_id, kSupervisedUserPasswordSchema, &value))
    result->SetKey(kSchemaVersion, base::Value(value));
  if (GetUserIntegerValue(user_id, kSupervisedUserPasswordRevision, &value))
    result->SetKey(kPasswordRevision, base::Value(value));

  bool flag;
  if (GetUserBooleanValue(user_id, kSupervisedUserNeedPasswordUpdate, &flag))
    result->SetKey(kRequirePasswordUpdate, base::Value(flag));
  if (GetUserBooleanValue(user_id, kSupervisedUserIncompleteKey, &flag))
    result->SetKey(kHasIncompleteKey, base::Value(flag));

  std::string salt;
  if (GetUserStringValue(user_id, kSupervisedUserPasswordSalt, &salt))
    result->SetKey(kSalt, base::Value(salt));
}

void SupervisedUserManagerImpl::SetPasswordInformation(
    const std::string& user_id,
    const base::DictionaryValue* password_info) {
  int value;
  if (password_info->GetIntegerWithoutPathExpansion(kSchemaVersion, &value))
    SetUserIntegerValue(user_id, kSupervisedUserPasswordSchema, value);
  if (password_info->GetIntegerWithoutPathExpansion(kPasswordRevision, &value))
    SetUserIntegerValue(user_id, kSupervisedUserPasswordRevision, value);

  bool flag;
  if (password_info->GetBooleanWithoutPathExpansion(kRequirePasswordUpdate,
                                                    &flag)) {
    SetUserBooleanValue(user_id, kSupervisedUserNeedPasswordUpdate, flag);
  }
  if (password_info->GetBooleanWithoutPathExpansion(kHasIncompleteKey, &flag))
    SetUserBooleanValue(user_id, kSupervisedUserIncompleteKey, flag);

  std::string salt;
  if (password_info->GetStringWithoutPathExpansion(kSalt, &salt))
    SetUserStringValue(user_id, kSupervisedUserPasswordSalt, salt);
  g_browser_process->local_state()->CommitPendingWrite();
}

bool SupervisedUserManagerImpl::GetUserStringValue(
    const std::string& user_id,
    const char* key,
    std::string* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dictionary = local_state->GetDictionary(key);
  return dictionary->GetStringWithoutPathExpansion(user_id, out_value);
}

bool SupervisedUserManagerImpl::GetUserIntegerValue(const std::string& user_id,
                                                    const char* key,
                                                    int* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dictionary = local_state->GetDictionary(key);
  return dictionary->GetIntegerWithoutPathExpansion(user_id, out_value);
}

bool SupervisedUserManagerImpl::GetUserBooleanValue(const std::string& user_id,
                                                    const char* key,
                                                    bool* out_value) const {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dictionary = local_state->GetDictionary(key);
  return dictionary->GetBooleanWithoutPathExpansion(user_id, out_value);
}

void SupervisedUserManagerImpl::SetUserStringValue(const std::string& user_id,
                                                   const char* key,
                                                   const std::string& value) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, key);
  update->SetKey(user_id, base::Value(value));
}

void SupervisedUserManagerImpl::SetUserIntegerValue(const std::string& user_id,
                                                    const char* key,
                                                    const int value) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, key);
  update->SetKey(user_id, base::Value(value));
}

void SupervisedUserManagerImpl::SetUserBooleanValue(const std::string& user_id,
                                                    const char* key,
                                                    const bool value) {
  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, key);
  update->SetKey(user_id, base::Value(value));
}

bool SupervisedUserManagerImpl::HasFailedUserCreationTransaction() {
  return !(g_browser_process->local_state()
               ->GetString(kSupervisedUserCreationTransactionDisplayName)
               .empty());
}

void SupervisedUserManagerImpl::RollbackUserCreationTransaction() {
  PrefService* prefs = g_browser_process->local_state();

  std::string display_name =
      prefs->GetString(kSupervisedUserCreationTransactionDisplayName);
  std::string user_id =
      prefs->GetString(kSupervisedUserCreationTransactionUserId);

  LOG(WARNING) << "Cleaning up transaction for " << display_name << "/"
               << user_id;

  if (user_id.empty()) {
    // Not much to do - just remove transaction.
    prefs->ClearPref(kSupervisedUserCreationTransactionDisplayName);
    prefs->CommitPendingWrite();
    return;
  }

  if (!owner_->IsSupervisedAccountId(AccountId::FromUserEmail(user_id))) {
    LOG(WARNING) << "Clean up transaction for non-supervised user found:"
                 << user_id << ", will not remove data";
    prefs->ClearPref(kSupervisedUserCreationTransactionDisplayName);
    prefs->ClearPref(kSupervisedUserCreationTransactionUserId);
    prefs->CommitPendingWrite();
    return;
  }
  owner_->RemoveNonOwnerUserInternal(AccountId::FromUserEmail(user_id),
                                     nullptr);

  prefs->ClearPref(kSupervisedUserCreationTransactionDisplayName);
  prefs->ClearPref(kSupervisedUserCreationTransactionUserId);
  prefs->CommitPendingWrite();
}

void SupervisedUserManagerImpl::RemoveNonCryptohomeData(
    const std::string& user_id) {
  PrefService* prefs = g_browser_process->local_state();
  ListPrefUpdate prefs_new_users_update(prefs, kSupervisedUsersFirstRun);
  prefs_new_users_update->Remove(base::Value(user_id), NULL);

  CleanPref(user_id, kSupervisedUserSyncId);
  CleanPref(user_id, kSupervisedUserManagers);
  CleanPref(user_id, kSupervisedUserManagerNames);
  CleanPref(user_id, kSupervisedUserManagerDisplayEmails);
  CleanPref(user_id, kSupervisedUserPasswordSalt);
  CleanPref(user_id, kSupervisedUserPasswordSchema);
  CleanPref(user_id, kSupervisedUserPasswordRevision);
  CleanPref(user_id, kSupervisedUserNeedPasswordUpdate);
  CleanPref(user_id, kSupervisedUserIncompleteKey);
}

void SupervisedUserManagerImpl::CleanPref(const std::string& user_id,
                                          const char* key) {
  PrefService* prefs = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(prefs, key);
  dict_update->RemoveWithoutPathExpansion(user_id, NULL);
}

bool SupervisedUserManagerImpl::CheckForFirstRun(const std::string& user_id) {
  ListPrefUpdate prefs_new_users_update(g_browser_process->local_state(),
                                        kSupervisedUsersFirstRun);
  return prefs_new_users_update->Remove(base::Value(user_id), NULL);
}

void SupervisedUserManagerImpl::UpdateManagerName(
    const std::string& manager_id,
    const base::string16& new_display_name) {
  PrefService* local_state = g_browser_process->local_state();

  const base::DictionaryValue* manager_ids =
      local_state->GetDictionary(kSupervisedUserManagers);

  DictionaryPrefUpdate manager_name_update(local_state,
                                           kSupervisedUserManagerNames);
  for (base::DictionaryValue::Iterator it(*manager_ids); !it.IsAtEnd();
       it.Advance()) {
    std::string user_id;
    bool has_manager_id = it.value().GetAsString(&user_id);
    DCHECK(has_manager_id);
    if (user_id == manager_id) {
      manager_name_update->SetWithoutPathExpansion(
          it.key(), std::make_unique<base::Value>(new_display_name));
    }
  }
}

SupervisedUserAuthentication* SupervisedUserManagerImpl::GetAuthentication() {
  return authentication_.get();
}

}  // namespace chromeos
