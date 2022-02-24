// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_manager/known_user.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace user_manager {
namespace {

// A vector pref of preferences of known users. All new preferences should be
// placed in this list.
const char kKnownUsers[] = "KnownUsers";

// Known user preferences keys (stored in Local State). All keys should be
// listed in kReservedKeys or kObsoleteKeys below.

// Key of canonical e-mail value.
const char kCanonicalEmail[] = "email";

// Key of obfuscated GAIA id value.
const char kGAIAIdKey[] = "gaia_id";

// Key of obfuscated object guid value for Active Directory accounts.
const char kObjGuidKey[] = "obj_guid";

// Key of account type.
const char kAccountTypeKey[] = "account_type";

// Key of whether this user ID refers to a SAML user.
const char kUsingSAMLKey[] = "using_saml";

// Key of whether this user authenticated via SAML using the principals API.
const char kIsUsingSAMLPrincipalsAPI[] = "using_saml_principals_api";

// Key of Device Id.
const char kDeviceId[] = "device_id";

// Key of GAPS cookie.
const char kGAPSCookie[] = "gaps_cookie";

// Key of the reason for re-auth.
const char kReauthReasonKey[] = "reauth_reason";

// Key for the GaiaId migration status.
const char kGaiaIdMigrationObsolete[] = "gaia_id_migration";

// Key of the boolean flag telling if a minimal user home migration has been
// attempted. This flag is not used since M88 and is only kept here to be able
// to remove it from existing entries.
const char kMinimalMigrationAttemptedObsolete[] = "minimal_migration_attempted";

// Key of the boolean flag telling if user session requires policy.
const char kProfileRequiresPolicy[] = "profile_requires_policy";

// Key of the boolean flag telling if user is ephemeral and should be removed
// from the local state on logout.
const char kIsEphemeral[] = "is_ephemeral";

// Key of the list value that stores challenge-response authentication keys.
const char kChallengeResponseKeys[] = "challenge_response_keys";

const char kLastOnlineSignin[] = "last_online_singin";
const char kOfflineSigninLimitObsolete[] = "offline_signin_limit";
const char kOfflineSigninLimit[] = "offline_signin_limit2";

// Key of the boolean flag telling if user is enterprise managed.
const char kIsEnterpriseManaged[] = "is_enterprise_managed";

// Key of the name of the entity (either a domain or email address) that manages
// the policies for this account.
const char kAccountManager[] = "enterprise_account_manager";

// Key of the last input method user used which is suitable for login/lock
// screen.
const char kLastInputMethod[] = "last_input_method";

// Key of the PIN auto submit length.
const char kPinAutosubmitLength[] = "pin_autosubmit_length";

// Key for the PIN auto submit backfill needed indicator.
const char kPinAutosubmitBackfillNeeded[] = "pin_autosubmit_backfill_needed";

// Sync token for SAML password multi-device sync
const char kPasswordSyncToken[] = "password_sync_token";

// Major version in which the user completed the onboarding flow.
const char kOnboardingCompletedVersion[] = "onboarding_completed_version";

// Last screen shown in the onboarding flow.
const char kPendingOnboardingScreen[] = "onboarding_screen_pending";

// List containing all the known user preferences keys.
const char* kReservedKeys[] = {kCanonicalEmail,
                               kGAIAIdKey,
                               kObjGuidKey,
                               kAccountTypeKey,
                               kUsingSAMLKey,
                               kIsUsingSAMLPrincipalsAPI,
                               kDeviceId,
                               kGAPSCookie,
                               kReauthReasonKey,
                               kProfileRequiresPolicy,
                               kIsEphemeral,
                               kChallengeResponseKeys,
                               kLastOnlineSignin,
                               kOfflineSigninLimit,
                               kIsEnterpriseManaged,
                               kAccountManager,
                               kLastInputMethod,
                               kPinAutosubmitLength,
                               kPinAutosubmitBackfillNeeded,
                               kPasswordSyncToken,
                               kOnboardingCompletedVersion,
                               kPendingOnboardingScreen};

// List containing all known user preference keys that used to be reserved and
// are now obsolete.
const char* kObsoleteKeys[] = {
    kMinimalMigrationAttemptedObsolete,
    kGaiaIdMigrationObsolete,
    kOfflineSigninLimitObsolete,
};

PrefService* GetLocalStateLegacy() {
  if (!UserManager::IsInitialized())
    return nullptr;

  return UserManager::Get()->GetLocalState();
}

// Checks if values in |dict| correspond with |account_id| identity.
bool UserMatches(const AccountId& account_id, const base::Value& dict) {
  const std::string* account_type = dict.FindStringKey(kAccountTypeKey);
  if (account_id.GetAccountType() != AccountType::UNKNOWN && account_type &&
      account_id.GetAccountType() !=
          AccountId::StringToAccountType(*account_type)) {
    return false;
  }

  // TODO(alemate): update code once user id is really a struct.
  // TODO(https://crbug.com/1190902): If the gaia id or GUID doesn't match,
  // this function should likely be returning false even if the e-mail matches.
  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE: {
      const std::string* gaia_id = dict.FindStringKey(kGAIAIdKey);
      if (gaia_id && account_id.GetGaiaId() == *gaia_id)
        return true;
      break;
    }
    case AccountType::ACTIVE_DIRECTORY: {
      const std::string* obj_guid = dict.FindStringKey(kObjGuidKey);
      if (obj_guid && account_id.GetObjGuid() == *obj_guid)
        return true;
      break;
    }
    case AccountType::UNKNOWN: {
    }
  }

  const std::string* email = dict.FindStringKey(kCanonicalEmail);
  if (email && account_id.GetUserEmail() == *email)
    return true;

  return false;
}

// Fills relevant |dict| values based on |account_id|.
// TODO(crbug.com/1287074): Consider a refactor to use base::Value::DictStorage.
// This would require |dict| to not be modified in-place.
void UpdateIdentity(const AccountId& account_id, base::Value& dict) {
  DCHECK(dict.is_dict());
  if (!account_id.GetUserEmail().empty())
    dict.SetStringKey(kCanonicalEmail, account_id.GetUserEmail());

  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE:
      if (!account_id.GetGaiaId().empty())
        dict.SetStringKey(kGAIAIdKey, account_id.GetGaiaId());
      break;
    case AccountType::ACTIVE_DIRECTORY:
      if (!account_id.GetObjGuid().empty())
        dict.SetStringKey(kObjGuidKey, account_id.GetObjGuid());
      break;
    case AccountType::UNKNOWN:
      return;
  }
  dict.SetStringKey(kAccountTypeKey, AccountId::AccountTypeToString(
                                         account_id.GetAccountType()));
}

}  // namespace

KnownUser::KnownUser(PrefService* local_state) : local_state_(local_state) {
  DCHECK(local_state);
}

KnownUser::~KnownUser() = default;

const base::Value* KnownUser::FindPrefs(const AccountId& account_id) const {
  // UserManager is usually NULL in unit tests.
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY &&
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id)) {
    return nullptr;
  }

  if (!account_id.is_valid())
    return nullptr;

  const base::Value* known_users = local_state_->GetList(kKnownUsers);
  for (const base::Value& element_value : known_users->GetListDeprecated()) {
    if (element_value.is_dict()) {
      if (UserMatches(account_id, element_value)) {
        return &element_value;
      }
    }
  }
  return nullptr;
}

void KnownUser::SetPath(const AccountId& account_id,
                        const std::string& path,
                        absl::optional<base::Value> opt_value) {
  // UserManager is usually NULL in unit tests.
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY &&
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id)) {
    return;
  }

  if (!account_id.is_valid())
    return;

  ListPrefUpdate update(local_state_, kKnownUsers);
  for (base::Value& element_value : update->GetListDeprecated()) {
    if (element_value.is_dict()) {
      if (UserMatches(account_id, element_value)) {
        if (opt_value.has_value())
          element_value.SetPath(path, std::move(opt_value).value());
        else
          element_value.RemovePath(path);

        UpdateIdentity(account_id, element_value);
        return;
      }
    }
  }
  if (!opt_value.has_value())
    return;

  base::Value new_dict(base::Value::Type::DICTIONARY);
  new_dict.SetPath(path, std::move(opt_value).value());
  UpdateIdentity(account_id, new_dict);
  update->Append(std::move(new_dict));
}

const std::string* KnownUser::FindStringPath(const AccountId& account_id,
                                             base::StringPiece path) const {
  const base::Value* user_pref_dict = FindPrefs(account_id);
  if (!user_pref_dict)
    return nullptr;

  return user_pref_dict->FindStringPath(path);
}

bool KnownUser::GetStringPrefForTest(const AccountId& account_id,
                                     const std::string& path,
                                     std::string* out_value) {
  const std::string* res = FindStringPath(account_id, path);
  if (out_value && res)
    *out_value = *res;
  return res;
}

void KnownUser::SetStringPref(const AccountId& account_id,
                              const std::string& path,
                              const std::string& in_value) {
  SetPath(account_id, path, base::Value(in_value));
}

absl::optional<bool> KnownUser::FindBoolPath(const AccountId& account_id,
                                             base::StringPiece path) const {
  const base::Value* user_pref_dict = FindPrefs(account_id);
  if (!user_pref_dict)
    return absl::nullopt;

  return user_pref_dict->FindBoolPath(path);
}

bool KnownUser::GetBooleanPrefForTest(const AccountId& account_id,
                                      const std::string& path,
                                      bool* out_value) {
  auto opt_val = FindBoolPath(account_id, path);
  if (out_value && opt_val.has_value())
    *out_value = opt_val.value();

  return opt_val.has_value();
}

void KnownUser::SetBooleanPref(const AccountId& account_id,
                               const std::string& path,
                               const bool in_value) {
  SetPath(account_id, path, base::Value(in_value));
}

absl::optional<int> KnownUser::FindIntPath(const AccountId& account_id,
                                           base::StringPiece path) const {
  const base::Value* user_pref_dict = FindPrefs(account_id);
  if (!user_pref_dict)
    return absl::nullopt;

  return user_pref_dict->FindIntPath(path);
}

bool KnownUser::GetIntegerPrefForTest(const AccountId& account_id,
                                      const std::string& path,
                                      int* out_value) {
  auto opt_val = FindIntPath(account_id, path);
  if (out_value && opt_val.has_value())
    *out_value = opt_val.value();

  return opt_val.has_value();
}

void KnownUser::SetIntegerPref(const AccountId& account_id,
                               const std::string& path,
                               const int in_value) {
  SetPath(account_id, path, base::Value(in_value));
}

bool KnownUser::GetPref(const AccountId& account_id,
                        const std::string& path,
                        const base::Value** out_value) {
  const base::Value* user_pref_dict = FindPrefs(account_id);
  if (!user_pref_dict)
    return false;

  *out_value = user_pref_dict->FindPath(path);
  return *out_value != nullptr;
}

void KnownUser::RemovePref(const AccountId& account_id,
                           const std::string& path) {
  // Prevent removing keys that are used internally.
  for (const std::string& key : kReservedKeys)
    CHECK_NE(path, key);

  SetPath(account_id, path, absl::nullopt);
}

AccountId KnownUser::GetAccountId(const std::string& user_email,
                                  const std::string& id,
                                  const AccountType& account_type) {
  DCHECK((id.empty() && account_type == AccountType::UNKNOWN) ||
         (!id.empty() && account_type != AccountType::UNKNOWN));
  // In tests empty accounts are possible.
  if (user_email.empty() && id.empty() &&
      account_type == AccountType::UNKNOWN) {
    return EmptyAccountId();
  }

  AccountId result(EmptyAccountId());
  // UserManager is usually NULL in unit tests.
  if (account_type == AccountType::UNKNOWN && UserManager::IsInitialized() &&
      UserManager::Get()->GetPlatformKnownUserId(user_email, id, &result)) {
    return result;
  }

  const std::string sanitized_email =
      user_email.empty()
          ? std::string()
          : gaia::CanonicalizeEmail(gaia::SanitizeEmail(user_email));

  if (!sanitized_email.empty()) {
    const AccountId account_id(AccountId::FromUserEmail(sanitized_email));
    if (const std::string* stored_gaia_id =
            FindStringPath(account_id, kGAIAIdKey)) {
      if (!id.empty()) {
        DCHECK(account_type == AccountType::GOOGLE);
        if (id != *stored_gaia_id)
          LOG(ERROR) << "User gaia id has changed. Sync will not work.";
      }

      // gaia_id is associated with cryptohome.
      return AccountId::FromUserEmailGaiaId(sanitized_email, *stored_gaia_id);
    }

    if (const std::string* stored_obj_guid =
            FindStringPath(account_id, kObjGuidKey)) {
      if (!id.empty()) {
        DCHECK(account_type == AccountType::ACTIVE_DIRECTORY);
        if (id != *stored_obj_guid)
          LOG(ERROR) << "User object guid has changed. Sync will not work.";
      }

      // obj_guid is associated with cryptohome.
      return AccountId::AdFromUserEmailObjGuid(sanitized_email,
                                               *stored_obj_guid);
    }
  }

  switch (account_type) {
    case AccountType::GOOGLE:
      if (const std::string* stored_email =
              FindStringPath(AccountId::FromGaiaId(id), kCanonicalEmail)) {
        return AccountId::FromUserEmailGaiaId(*stored_email, id);
      }
      return AccountId::FromUserEmailGaiaId(sanitized_email, id);
    case AccountType::ACTIVE_DIRECTORY:
      if (const std::string* stored_email =
              FindStringPath(AccountId::AdFromObjGuid(id), kCanonicalEmail)) {
        return AccountId::AdFromUserEmailObjGuid(*stored_email, id);
      }
      return AccountId::AdFromUserEmailObjGuid(sanitized_email, id);
    case AccountType::UNKNOWN:
      return AccountId::FromUserEmail(sanitized_email);
  }
  NOTREACHED();
  return EmptyAccountId();
}

std::vector<AccountId> KnownUser::GetKnownAccountIds() {
  std::vector<AccountId> result;

  const base::Value* known_users = local_state_->GetList(kKnownUsers);
  for (const base::Value& element_value : known_users->GetListDeprecated()) {
    if (element_value.is_dict()) {
      const std::string* email = element_value.FindStringKey(kCanonicalEmail);
      const std::string* gaia_id = element_value.FindStringKey(kGAIAIdKey);
      const std::string* obj_guid = element_value.FindStringKey(kObjGuidKey);
      AccountType account_type = AccountType::GOOGLE;
      if (const std::string* account_type_string =
              element_value.FindStringKey(kAccountTypeKey)) {
        account_type = AccountId::StringToAccountType(*account_type_string);
      }
      switch (account_type) {
        case AccountType::GOOGLE:
          if (email || gaia_id) {
            result.push_back(AccountId::FromUserEmailGaiaId(
                email ? *email : std::string(),
                gaia_id ? *gaia_id : std::string()));
          }
          break;
        case AccountType::ACTIVE_DIRECTORY:
          if (email && obj_guid) {
            result.push_back(
                AccountId::AdFromUserEmailObjGuid(*email, *obj_guid));
          }
          break;
        default:
          NOTREACHED() << "Unknown account type";
      }
    }
  }
  return result;
}

void KnownUser::SaveKnownUser(const AccountId& account_id) {
  const bool is_ephemeral =
      UserManager::IsInitialized() &&
      UserManager::Get()->IsUserNonCryptohomeDataEphemeral(account_id);
  if (is_ephemeral &&
      account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY) {
    return;
  }
  UpdateId(account_id);
  local_state_->CommitPendingWrite();
}

void KnownUser::SetIsEphemeralUser(const AccountId& account_id,
                                   bool is_ephemeral) {
  if (account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY)
    return;
  SetBooleanPref(account_id, kIsEphemeral, is_ephemeral);
}

void KnownUser::UpdateId(const AccountId& account_id) {
  switch (account_id.GetAccountType()) {
    case AccountType::GOOGLE:
      SetStringPref(account_id, kGAIAIdKey, account_id.GetGaiaId());
      break;
    case AccountType::ACTIVE_DIRECTORY:
      SetStringPref(account_id, kObjGuidKey, account_id.GetObjGuid());
      break;
    case AccountType::UNKNOWN:
      return;
  }
  SetStringPref(account_id, kAccountTypeKey,
                AccountId::AccountTypeToString(account_id.GetAccountType()));
}

const std::string* KnownUser::FindGaiaID(const AccountId& account_id) {
  return FindStringPath(account_id, kGAIAIdKey);
}

void KnownUser::SetDeviceId(const AccountId& account_id,
                            const std::string& device_id) {
  const std::string known_device_id = GetDeviceId(account_id);
  if (!known_device_id.empty() && device_id != known_device_id) {
    NOTREACHED() << "Trying to change device ID for known user.";
  }
  SetStringPref(account_id, kDeviceId, device_id);
}

std::string KnownUser::GetDeviceId(const AccountId& account_id) {
  const std::string* device_id = FindStringPath(account_id, kDeviceId);
  if (device_id)
    return *device_id;
  return std::string();
}

void KnownUser::SetGAPSCookie(const AccountId& account_id,
                              const std::string& gaps_cookie) {
  SetStringPref(account_id, kGAPSCookie, gaps_cookie);
}

std::string KnownUser::GetGAPSCookie(const AccountId& account_id) {
  const std::string* gaps_cookie = FindStringPath(account_id, kGAPSCookie);
  if (gaps_cookie)
    return *gaps_cookie;
  return std::string();
}

void KnownUser::UpdateUsingSAML(const AccountId& account_id,
                                const bool using_saml) {
  SetBooleanPref(account_id, kUsingSAMLKey, using_saml);
}

bool KnownUser::IsUsingSAML(const AccountId& account_id) {
  return FindBoolPath(account_id, kUsingSAMLKey).value_or(false);
}

void KnownUser::UpdateIsUsingSAMLPrincipalsAPI(
    const AccountId& account_id,
    bool is_using_saml_principals_api) {
  SetBooleanPref(account_id, kIsUsingSAMLPrincipalsAPI,
                 is_using_saml_principals_api);
}

bool KnownUser::GetIsUsingSAMLPrincipalsAPI(const AccountId& account_id) {
  return FindBoolPath(account_id, kIsUsingSAMLPrincipalsAPI).value_or(false);
}

void KnownUser::SetProfileRequiresPolicy(const AccountId& account_id,
                                         ProfileRequiresPolicy required) {
  DCHECK_NE(required, ProfileRequiresPolicy::kUnknown);
  SetBooleanPref(account_id, kProfileRequiresPolicy,
                 required == ProfileRequiresPolicy::kPolicyRequired);
}

ProfileRequiresPolicy KnownUser::GetProfileRequiresPolicy(
    const AccountId& account_id) {
  absl::optional<bool> requires_policy =
      FindBoolPath(account_id, kProfileRequiresPolicy);
  if (requires_policy.has_value()) {
    return requires_policy.value() ? ProfileRequiresPolicy::kPolicyRequired
                                   : ProfileRequiresPolicy::kNoPolicyRequired;
  }
  return ProfileRequiresPolicy::kUnknown;
}

void KnownUser::ClearProfileRequiresPolicy(const AccountId& account_id) {
  SetPath(account_id, kProfileRequiresPolicy, absl::nullopt);
}

void KnownUser::UpdateReauthReason(const AccountId& account_id,
                                   const int reauth_reason) {
  SetIntegerPref(account_id, kReauthReasonKey, reauth_reason);
}

absl::optional<int> KnownUser::FindReauthReason(
    const AccountId& account_id) const {
  return FindIntPath(account_id, kReauthReasonKey);
}

void KnownUser::SetChallengeResponseKeys(const AccountId& account_id,
                                         base::Value value) {
  DCHECK(value.is_list());
  SetPath(account_id, kChallengeResponseKeys, std::move(value));
}

base::Value KnownUser::GetChallengeResponseKeys(const AccountId& account_id) {
  const base::Value* value = nullptr;
  if (!GetPref(account_id, kChallengeResponseKeys, &value) || !value->is_list())
    return base::Value();
  return value->Clone();
}

void KnownUser::SetLastOnlineSignin(const AccountId& account_id,
                                    base::Time time) {
  SetPath(account_id, kLastOnlineSignin, base::TimeToValue(time));
}

base::Time KnownUser::GetLastOnlineSignin(const AccountId& account_id) {
  const base::Value* value = nullptr;
  if (!GetPref(account_id, kLastOnlineSignin, &value))
    return base::Time();
  absl::optional<base::Time> time = base::ValueToTime(value);
  if (!time)
    return base::Time();
  return *time;
}

void KnownUser::SetOfflineSigninLimit(
    const AccountId& account_id,
    absl::optional<base::TimeDelta> time_delta) {
  if (!time_delta) {
    SetPath(account_id, kOfflineSigninLimit, absl::nullopt);
  } else {
    SetPath(account_id, kOfflineSigninLimit,
            base::TimeDeltaToValue(time_delta.value()));
  }
}

absl::optional<base::TimeDelta> KnownUser::GetOfflineSigninLimit(
    const AccountId& account_id) {
  const base::Value* value = nullptr;
  if (!GetPref(account_id, kOfflineSigninLimit, &value))
    return absl::nullopt;
  absl::optional<base::TimeDelta> time_delta = base::ValueToTimeDelta(value);
  return time_delta;
}

void KnownUser::SetIsEnterpriseManaged(const AccountId& account_id,
                                       bool is_enterprise_managed) {
  SetBooleanPref(account_id, kIsEnterpriseManaged, is_enterprise_managed);
}

bool KnownUser::GetIsEnterpriseManaged(const AccountId& account_id) {
  return FindBoolPath(account_id, kIsEnterpriseManaged).value_or(false);
}

void KnownUser::SetAccountManager(const AccountId& account_id,
                                  const std::string& manager) {
  SetStringPref(account_id, kAccountManager, manager);
}

const std::string* KnownUser::GetAccountManager(const AccountId& account_id) {
  return FindStringPath(account_id, kAccountManager);
}

void KnownUser::SetUserLastLoginInputMethodId(
    const AccountId& account_id,
    const std::string& input_method_id) {
  SetStringPref(account_id, kLastInputMethod, input_method_id);
}

const std::string* KnownUser::GetUserLastInputMethodId(
    const AccountId& account_id) {
  return FindStringPath(account_id, kLastInputMethod);
}

void KnownUser::SetUserPinLength(const AccountId& account_id, int pin_length) {
  SetIntegerPref(account_id, kPinAutosubmitLength, pin_length);
}

int KnownUser::GetUserPinLength(const AccountId& account_id) {
  return FindIntPath(account_id, kPinAutosubmitLength).value_or(0);
}

bool KnownUser::PinAutosubmitIsBackfillNeeded(const AccountId& account_id) {
  // If the pref is not set, the pref needs to be backfilled.
  return FindBoolPath(account_id, kPinAutosubmitBackfillNeeded).value_or(true);
}

void KnownUser::PinAutosubmitSetBackfillNotNeeded(const AccountId& account_id) {
  SetBooleanPref(account_id, kPinAutosubmitBackfillNeeded, false);
}

void KnownUser::PinAutosubmitSetBackfillNeededForTests(
    const AccountId& account_id) {
  SetBooleanPref(account_id, kPinAutosubmitBackfillNeeded, true);
}

void KnownUser::SetPasswordSyncToken(const AccountId& account_id,
                                     const std::string& token) {
  SetStringPref(account_id, kPasswordSyncToken, token);
}

const std::string* KnownUser::GetPasswordSyncToken(
    const AccountId& account_id) const {
  return FindStringPath(account_id, kPasswordSyncToken);
}

void KnownUser::SetOnboardingCompletedVersion(
    const AccountId& account_id,
    const absl::optional<base::Version> version) {
  if (!version) {
    SetPath(account_id, kOnboardingCompletedVersion, absl::nullopt);
  } else {
    SetStringPref(account_id, kOnboardingCompletedVersion,
                  version.value().GetString());
  }
}

absl::optional<base::Version> KnownUser::GetOnboardingCompletedVersion(
    const AccountId& account_id) {
  const std::string* str_version =
      FindStringPath(account_id, kOnboardingCompletedVersion);

  if (!str_version)
    return absl::nullopt;

  base::Version version = base::Version(*str_version);
  if (!version.IsValid())
    return absl::nullopt;
  return version;
}

void KnownUser::RemoveOnboardingCompletedVersionForTests(
    const AccountId& account_id) {
  SetPath(account_id, kOnboardingCompletedVersion, absl::nullopt);
}

void KnownUser::SetPendingOnboardingScreen(const AccountId& account_id,
                                           const std::string& screen) {
  SetStringPref(account_id, kPendingOnboardingScreen, screen);
}

void KnownUser::RemovePendingOnboardingScreen(const AccountId& account_id) {
  SetPath(account_id, kPendingOnboardingScreen, absl::nullopt);
}

std::string KnownUser::GetPendingOnboardingScreen(const AccountId& account_id) {
  if (const std::string* screen =
          FindStringPath(account_id, kPendingOnboardingScreen)) {
    return *screen;
  }
  // Return empty string if no screen is pending.
  return std::string();
}

bool KnownUser::UserExists(const AccountId& account_id) {
  return FindPrefs(account_id);
}

void KnownUser::RemovePrefs(const AccountId& account_id) {
  if (!account_id.is_valid())
    return;

  ListPrefUpdate update(local_state_, kKnownUsers);
  base::Value::ListView update_view = update->GetListDeprecated();
  for (auto it = update_view.begin(); it != update_view.end(); ++it) {
    if (UserMatches(account_id, *it)) {
      update->EraseListIter(it);
      break;
    }
  }
}

void KnownUser::CleanEphemeralUsers() {
  ListPrefUpdate update(local_state_, kKnownUsers);
  update->EraseListValueIf([](const auto& value) {
    if (!value.is_dict())
      return false;

    absl::optional<bool> is_ephemeral = value.FindBoolKey(kIsEphemeral);
    return is_ephemeral && *is_ephemeral;
  });
}

void KnownUser::CleanObsoletePrefs() {
  ListPrefUpdate update(local_state_, kKnownUsers);
  for (base::Value& user_entry : update.Get()->GetListDeprecated()) {
    if (!user_entry.is_dict())
      continue;
    for (const std::string& key : kObsoleteKeys)
      user_entry.RemoveKey(key);
  }
}

// static
void KnownUser::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(kKnownUsers);
}

// --- Legacy interface ---
namespace known_user {

void SetStringPref(const AccountId& account_id,
                   const std::string& path,
                   const std::string& in_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetStringPref(account_id, path, in_value);
}

void SetBooleanPref(const AccountId& account_id,
                    const std::string& path,
                    const bool in_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetBooleanPref(account_id, path, in_value);
}

bool GetIntegerPref(const AccountId& account_id,
                    const std::string& path,
                    int* out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  absl::optional<int> val =
      KnownUser(local_state).FindIntPath(account_id, path);
  if (out_value && val.has_value())
    *out_value = val.value();
  return val.has_value();
}

void SetIntegerPref(const AccountId& account_id,
                    const std::string& path,
                    const int in_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetIntegerPref(account_id, path, in_value);
}

bool GetPref(const AccountId& account_id,
             const std::string& path,
             const base::Value** out_value) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return false;
  return KnownUser(local_state).GetPref(account_id, path, out_value);
}

void RemovePref(const AccountId& account_id, const std::string& path) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).RemovePref(account_id, path);
}

AccountId GetAccountId(const std::string& user_email,
                       const std::string& id,
                       const AccountType& account_type) {
  DCHECK((id.empty() && account_type == AccountType::UNKNOWN) ||
         (!id.empty() && account_type != AccountType::UNKNOWN));
  PrefService* local_state = GetLocalStateLegacy();
  if (local_state) {
    return KnownUser(local_state).GetAccountId(user_email, id, account_type);
  }

  // The handling of the local-state-not-initialized case is pretty complex - it
  // is KnownUser::GetAccountId with all queries assuming to return false.
  // This should be come unnecessary when all callers are migrated to the
  // KnownUser class interface (https://crbug.com/1150434) and thus responsible
  // to pass a valid |local_state| pointer.

  // In tests empty accounts are possible.
  if (user_email.empty() && id.empty() &&
      account_type == AccountType::UNKNOWN) {
    return EmptyAccountId();
  }
  AccountId result(EmptyAccountId());
  // UserManager is usually NULL in unit tests.
  if (account_type == AccountType::UNKNOWN && UserManager::IsInitialized() &&
      UserManager::Get()->GetPlatformKnownUserId(user_email, id, &result)) {
    return result;
  }
  const std::string sanitized_email =
      user_email.empty()
          ? std::string()
          : gaia::CanonicalizeEmail(gaia::SanitizeEmail(user_email));
  std::string stored_email;
  switch (account_type) {
    case AccountType::GOOGLE:
      return AccountId::FromUserEmailGaiaId(sanitized_email, id);
    case AccountType::ACTIVE_DIRECTORY:
      return AccountId::AdFromUserEmailObjGuid(sanitized_email, id);
    case AccountType::UNKNOWN:
      return AccountId::FromUserEmail(sanitized_email);
  }
  NOTREACHED();
  return EmptyAccountId();
}

std::vector<AccountId> GetKnownAccountIds() {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return {};
  return KnownUser(local_state).GetKnownAccountIds();
}

void UpdateId(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).UpdateId(account_id);
}

void SetDeviceId(const AccountId& account_id, const std::string& device_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return;
  return KnownUser(local_state).SetDeviceId(account_id, device_id);
}

std::string GetDeviceId(const AccountId& account_id) {
  PrefService* local_state = GetLocalStateLegacy();
  // Local State may not be initialized in tests.
  if (!local_state)
    return std::string();
  return KnownUser(local_state).GetDeviceId(account_id);
}

}  // namespace known_user
}  // namespace user_manager
