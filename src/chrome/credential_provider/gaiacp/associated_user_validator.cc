// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/associated_user_validator.h"

#include <ntstatus.h>
#include <process.h>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "base/win/win_util.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/internet_availability_checker.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "chrome/credential_provider/gaiacp/os_user_manager.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/gaiacp/win_http_url_fetcher.h"

namespace credential_provider {

const base::TimeDelta
    AssociatedUserValidator::kDefaultTokenHandleValidationTimeout =
        base::TimeDelta::FromMilliseconds(3000);

const base::TimeDelta AssociatedUserValidator::kTokenHandleValidityLifetime =
    base::TimeDelta::FromSeconds(30);

const char AssociatedUserValidator::kTokenInfoUrl[] =
    "https://www.googleapis.com/oauth2/v2/tokeninfo";

namespace {

struct CheckReauthParams {
  base::string16 sid;
  base::string16 token_handle;
  std::unique_ptr<WinHttpUrlFetcher> fetcher;
};

// Queries google to see whether the user's token handle is no longer valid or
// is expired. Returns 1 if it is still valid, 0 if the user needs to reauth.
unsigned __stdcall CheckReauthStatus(void* param) {
  DCHECK(param);
  std::unique_ptr<CheckReauthParams> reauth_info(
      reinterpret_cast<CheckReauthParams*>(param));

  if (reauth_info->fetcher) {
    reauth_info->fetcher->SetRequestHeader("Content-Type",
                                           "application/x-www-form-urlencoded");

    std::string body = base::StringPrintf("token_handle=%ls",
                                          reauth_info->token_handle.c_str());
    HRESULT hr = reauth_info->fetcher->SetRequestBody(body.c_str());
    if (FAILED(hr)) {
      LOGFN(ERROR) << "fetcher.SetRequestBody sid=" << reauth_info->sid
                   << " hr=" << putHR(hr);
      return 1;
    }

    std::vector<char> response;
    hr = reauth_info->fetcher->Fetch(&response);
    if (FAILED(hr)) {
      LOGFN(INFO) << "fetcher.Fetch sid=" << reauth_info->sid
                  << " hr=" << putHR(hr);
      return 1;
    }

    base::StringPiece response_string(response.data(), response.size());
    base::Optional<base::Value> properties(base::JSONReader::Read(
        response_string, base::JSON_ALLOW_TRAILING_COMMAS));
    if (!properties || !properties->is_dict()) {
      LOGFN(ERROR) << "base::JSONReader::Read failed forcing reauth";
      return 0;
    }

    base::Optional<int> expires_in = properties->FindIntKey("expires_in");
    if (properties->FindKey("error") || !expires_in || expires_in.value() < 0) {
      LOGFN(INFO) << "Needs reauth sid=" << reauth_info->sid;
      return 0;
    }
  }

  return 1;
}

bool TokenHandleNeedsUpdate(const base::Time& last_refresh) {
  return (base::Time::Now() - last_refresh) >
         AssociatedUserValidator::kTokenHandleValidityLifetime;
}

bool WaitForQueryResult(const base::win::ScopedHandle& thread_handle,
                        const base::Time& until) {
  if (!thread_handle.IsValid())
    return true;

  DWORD time_left = std::max<DWORD>(
      static_cast<DWORD>((until - base::Time::Now()).InMilliseconds()), 0);

  // See if a response to the token info can be fetched in a reasonable
  // amount of time. If not, assume there is no internet and that the handle
  // is still valid.
  HRESULT hr = ::WaitForSingleObject(thread_handle.Get(), time_left);

  bool token_handle_validity = false;
  if (hr == WAIT_OBJECT_0) {
    DWORD exit_code;
    token_handle_validity =
        !::GetExitCodeThread(thread_handle.Get(), &exit_code) || exit_code == 1;
  } else if (hr == WAIT_TIMEOUT) {
    token_handle_validity = true;
  }

  return token_handle_validity;
}

HRESULT ModifyUserAccess(const std::unique_ptr<ScopedLsaPolicy>& policy,
                         const base::string16& sid,
                         bool allow) {
  OSUserManager* manager = OSUserManager::Get();
  wchar_t username[kWindowsUsernameBufferLength];
  wchar_t domain[kWindowsDomainBufferLength];

  HRESULT hr = manager->FindUserBySID(
      sid.c_str(), username, base::size(username), domain, base::size(domain));

  if (FAILED(hr)) {
    LOGFN(ERROR) << "FindUserBySID sid=" << sid << " hr=" << putHR(hr);
    return hr;
  }

  return manager->ModifyUserAccessWithLogonHours(domain, username, allow);
}

}  // namespace

AssociatedUserValidator::TokenHandleInfo::TokenHandleInfo() = default;
AssociatedUserValidator::TokenHandleInfo::~TokenHandleInfo() = default;

AssociatedUserValidator::TokenHandleInfo::TokenHandleInfo(
    const base::string16& token_handle)
    : queried_token_handle(token_handle), last_update(base::Time::Now()) {}

AssociatedUserValidator::TokenHandleInfo::TokenHandleInfo(
    const base::string16& token_handle,
    base::Time update_time,
    base::win::ScopedHandle::Handle thread_handle)
    : queried_token_handle(token_handle),
      last_update(update_time),
      pending_query_thread(thread_handle) {}

AssociatedUserValidator::ScopedBlockDenyAccessUpdate::
    ScopedBlockDenyAccessUpdate(AssociatedUserValidator* validator)
    : validator_(validator) {
  DCHECK(validator_);
  validator_->BlockDenyAccessUpdate();
}

AssociatedUserValidator::ScopedBlockDenyAccessUpdate::
    ~ScopedBlockDenyAccessUpdate() {
  validator_->UnblockDenyAccessUpdate();
}

// static
AssociatedUserValidator* AssociatedUserValidator::Get() {
  return *GetInstanceStorage();
}

// static
AssociatedUserValidator** AssociatedUserValidator::GetInstanceStorage() {
  static AssociatedUserValidator instance(kDefaultTokenHandleValidationTimeout);
  static AssociatedUserValidator* instance_storage = &instance;

  return &instance_storage;
}

AssociatedUserValidator::AssociatedUserValidator(
    base::TimeDelta validation_timeout)
    : validation_timeout_(validation_timeout) {}

AssociatedUserValidator::~AssociatedUserValidator() = default;

bool AssociatedUserValidator::HasInternetConnection() const {
  return InternetAvailabilityChecker::Get()->HasInternetConnection();
}

HRESULT AssociatedUserValidator::UpdateAssociatedSids(
    std::map<base::string16, base::string16>* sid_to_handle) {
  std::map<base::string16, UserTokenHandleInfo> sids_to_handle_info;

  HRESULT hr = GetUserTokenHandles(&sids_to_handle_info);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "GetUserTokenHandles hr=" << putHR(hr);
    return hr;
  }

  std::set<base::string16> users_to_delete;
  OSUserManager* manager = OSUserManager::Get();
  for (const auto& sid_to_association : sids_to_handle_info) {
    const base::string16& sid = sid_to_association.first;
    const UserTokenHandleInfo& info = sid_to_association.second;
    if (info.gaia_id.empty()) {
      users_to_delete.insert(sid_to_association.first);
      continue;
    }
    HRESULT hr = manager->FindUserBySID(sid.c_str(), nullptr, 0, nullptr, 0);
    if (hr == HRESULT_FROM_WIN32(ERROR_NONE_MAPPED)) {
      users_to_delete.insert(sid_to_association.first);
      continue;
    } else if (FAILED(hr)) {
      LOGFN(ERROR) << "manager->FindUserBySID hr=" << putHR(hr);
    }

    if (sid_to_handle)
      sid_to_handle->emplace(sid, info.token_handle);
  }

  for (const auto& to_delete : users_to_delete) {
    user_to_token_handle_info_.erase(to_delete);
    RemoveAllUserProperties(to_delete);
  }

  return S_OK;
}

size_t AssociatedUserValidator::GetAssociatedUsersCount() {
  base::AutoLock locker(validator_lock_);

  UpdateAssociatedSids(nullptr);

  return user_to_token_handle_info_.size();
}

bool AssociatedUserValidator::IsUserAccessBlockingEnforced(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) const {
  if (!MdmEnrollmentEnabled())
    return false;

  if (!CGaiaCredentialProvider::IsUsageScenarioSupported(cpus))
    return false;

  if (!HasInternetConnection())
    return false;

  return true;
}

bool AssociatedUserValidator::DenySigninForUsersWithInvalidTokenHandles(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
  base::AutoLock locker(validator_lock_);

  if (block_deny_access_update_) {
    LOGFN(INFO) << "Block the deny access update";
    return false;
  }

  if (!IsUserAccessBlockingEnforced(cpus))
    return false;

  HRESULT hr = UpdateAssociatedSids(nullptr);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "UpdateAssociatedSids hr=" << putHR(hr);
    return false;
  }

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);

  bool user_denied_signin = false;
  OSUserManager* manager = OSUserManager::Get();
  for (const auto& user_info : user_to_token_handle_info_) {
    const base::string16& sid = user_info.first;
    if (locked_user_sids_.find(sid) != locked_user_sids_.end())
      continue;

    // Note that logon hours cannot be changed on domain joined AD user account.
    if (!IsTokenHandleValidForUserInternal(sid) &&
        !manager->IsUserDomainJoined(sid)) {
      LOGFN(INFO) << "Revoking access for sid=" << sid;
      HRESULT hr = ModifyUserAccess(policy, sid, false);
      if (FAILED(hr)) {
        LOGFN(ERROR) << "ModifyUserAccess sid=" << sid << " hr=" << putHR(hr);
      } else {
        locked_user_sids_.insert(sid);
        user_denied_signin = true;
      }
    } else if (manager->IsUserDomainJoined(sid)) {
      // TODO(crbug.com/973160): Description provided in the bug.
      LOGFN(INFO) << "Not denying signin for AD user accounts.";
    }
  }

  return user_denied_signin;
}

HRESULT AssociatedUserValidator::RestoreUserAccess(const base::string16& sid) {
  base::AutoLock locker(validator_lock_);

  if (locked_user_sids_.erase(sid)) {
    auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
    return ModifyUserAccess(policy, sid, true);
  }

  return S_OK;
}

void AssociatedUserValidator::AllowSigninForAllAssociatedUsers(
    CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus) {
  base::AutoLock locker(validator_lock_);

  if (!MdmEnrollmentEnabled() ||
      !CGaiaCredentialProvider::IsUsageScenarioSupported(cpus))
    return;

  std::map<base::string16, base::string16> sids_to_handle;
  HRESULT hr = UpdateAssociatedSids(&sids_to_handle);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "UpdateAssociatedSids hr=" << putHR(hr);
    return;
  }

  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  for (const auto& sid_to_handle : sids_to_handle)
    ModifyUserAccess(policy, sid_to_handle.first, true);

  locked_user_sids_.clear();
}

void AssociatedUserValidator::AllowSigninForUsersWithInvalidTokenHandles() {
  base::AutoLock locker(validator_lock_);

  LOGFN(INFO);
  auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
  for (auto& sid : locked_user_sids_) {
    HRESULT hr = ModifyUserAccess(policy, sid, true);
    if (FAILED(hr))
      LOGFN(ERROR) << "ModifyUserAccess sid=" << sid << " hr=" << putHR(hr);
  }
  locked_user_sids_.clear();
}

void AssociatedUserValidator::StartRefreshingTokenHandleValidity() {
  base::AutoLock locker(validator_lock_);

  std::map<base::string16, base::string16> sid_to_handle;
  HRESULT hr = UpdateAssociatedSids(&sid_to_handle);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "UpdateAssociatedSids hr=" << putHR(hr);
    return;
  }

  // Fire off the threads that will query the token handles but do not wait for
  // them to complete. Later queries will do the wait.
  CheckTokenHandleValidity(sid_to_handle);
}

void AssociatedUserValidator::CheckTokenHandleValidity(
    const std::map<base::string16, base::string16>& handles_to_verify) {
  for (auto it = handles_to_verify.cbegin(); it != handles_to_verify.cend();
       ++it) {
    // Make sure the user actually exists.
    if (FAILED(OSUserManager::Get()->FindUserBySID(it->first.c_str(), nullptr,
                                                   0, nullptr, 0))) {
      continue;
    }

    // User exists, has a gaia id, but no token handle. Consider this an invalid
    // token handle and the user needs to sign in with Gaia to get a new one.
    if (it->second.empty()) {
      user_to_token_handle_info_[it->first] =
          std::make_unique<TokenHandleInfo>(base::string16());
      continue;
    }

    // If there is already token handle info for the current user and it
    // 1. Is NOT valid.
    // AND
    // 2. There is no query still pending on it.
    // Then this means that the user has an invalid token handle.
    // The state of this handle will never change during the execution of the
    // sign in process (since a new token handle will only be written on
    // successful sign in) so it does not need to update and the current state
    // information is valid.
    auto existing_validity_it = user_to_token_handle_info_.find(it->first);
    if (existing_validity_it != user_to_token_handle_info_.end() &&
        !existing_validity_it->second->is_valid &&
        !existing_validity_it->second->pending_query_thread.IsValid()) {
      continue;
    }

    // Start a new token handle query if:
    // 1. No token info entry yet exists for this token handle.
    // OR
    // 2. Token info exists but it is stale (either because it is a query that
    // was started a while ago but for which we never tried to query the result
    // because the last query result is from a while ago or because the last
    // query result is older than |kTokenHandleValidityLifetime|).
    if (existing_validity_it == user_to_token_handle_info_.end() ||
        TokenHandleNeedsUpdate(existing_validity_it->second->last_update)) {
      StartTokenValidityQuery(it->first, it->second, validation_timeout_);
    }
  }
}

void AssociatedUserValidator::StartTokenValidityQuery(
    const base::string16& sid,
    const base::string16& token_handle,
    base::TimeDelta timeout) {
  base::Time max_end_time = base::Time::Now() + timeout;

  // Fire off a thread to check with Gaia if a re-auth is required. The thread
  // does not reference this object nor does this object have any reference
  // directly on the thread. This object only checks for the return code of the
  // thread within a given timeout. If no return code is given in that timeout
  // then assume that the token handle is valid. The running thread can continue
  // running and finish its execution without worrying about notifying anything
  // about the result.
  unsigned wait_thread_id;
  CheckReauthParams* params = new CheckReauthParams{
      sid, token_handle,
      WinHttpUrlFetcher::Create(GURL(AssociatedUserValidator::kTokenInfoUrl))};
  uintptr_t wait_thread =
      _beginthreadex(nullptr, 0, CheckReauthStatus,
                     reinterpret_cast<void*>(params), 0, &wait_thread_id);
  if (wait_thread == 0) {
    user_to_token_handle_info_[sid] =
        std::make_unique<TokenHandleInfo>(token_handle);
    delete params;
    return;
  }

  user_to_token_handle_info_[sid] = std::make_unique<TokenHandleInfo>(
      token_handle, max_end_time, reinterpret_cast<HANDLE>(wait_thread));
}

bool AssociatedUserValidator::IsTokenHandleValidForUser(
    const base::string16& sid) {
  base::AutoLock locker(validator_lock_);
  return IsTokenHandleValidForUserInternal(sid);
}

bool AssociatedUserValidator::IsTokenHandleValidForUserInternal(
    const base::string16& sid) {
  // All token handles are valid when no internet connection is available.
  if (!HasInternetConnection())
    return true;

  // If at this point there is no token info entry for this user, assume the
  // user is not associated and does not need a token handle and is thus always
  // valid. On initial startup we should have already called
  // StartRefreshingTokenHandleValidity to create all the token info for all the
  // current associated users. Between the first creation of all the token infos
  // and the eventual successful  sign in there should be no new token handles
  // created so we can immediately assign that an absence of token handle info
  // for this user means that they are not associated and do not need to
  // validate any token handles.
  auto validity_it = user_to_token_handle_info_.find(sid);

  if (validity_it == user_to_token_handle_info_.end())
    return true;

  // If mdm enrollment is needed, then force a reauth for all users so
  // that they enroll.
  if (NeedsToEnrollWithMdm())
    return false;

  if (MdmPasswordRecoveryEnabled()) {
    base::string16 store_key = GetUserPasswordLsaStoreKey(sid);
    auto policy = ScopedLsaPolicy::Create(POLICY_ALL_ACCESS);
    if (!policy->PrivateDataExists(store_key.c_str())) {
      LOGFN(INFO) << "Enforcing re-auth due to missing password lsa store "
                     "data for user "
                  << sid;
      return false;
    }
  }

  // This function will start a new query if the current info for the token
  // handle is stale or has not yet been queried. At the end of this function,
  // either we will already have the validity of the token handle or we have a
  // handle to a pending query that we wait to complete before finally having
  // the validity.
  CheckTokenHandleValidity({{sid, validity_it->second->queried_token_handle}});

  // If a query is still pending, wait for it and update the validity.
  if (validity_it->second->pending_query_thread.IsValid()) {
    validity_it->second->is_valid =
        WaitForQueryResult(validity_it->second->pending_query_thread,
                           validity_it->second->last_update);
    if (!validity_it->second->is_valid) {
      // Clear the token handle to delete it. At this point we know it is
      // definetely invalid so if we remove the handle completely we will
      // no longer need to query it and can just assume immediately that
      // the user needs to be reauthorized.
      HRESULT hr = SetUserProperty(sid, kUserTokenHandle, L"");
      if (FAILED(hr))
        LOGFN(ERROR) << "SetUserProperty hr=" << putHR(hr);
    }
    validity_it->second->pending_query_thread.Close();
    base::Time now = base::Time::Now();
    // NOTE: Don't always update |last_update| because the result of this query
    // may still be old. E.g.:
    // 1. At time X thread is started to query. The maximum end time of this
    //    query is X + timeout
    // 2. At time Y (X + timeout + lifetime > Y >> X + timeout) we ask for
    //    the validity of the token handle. The time Y might still be valid
    //    when considering the lifetime but may still be relatively old
    //    depending on how long after time X the request is made. So keep the
    //    original end time of the query as the last update (if the end time
    //    occurs before time Y) so that the token handle is updated earlier on
    //    the next query.
    validity_it->second->last_update = now > validity_it->second->last_update
                                           ? validity_it->second->last_update
                                           : now;
  }

  return validity_it->second->is_valid;
}

void AssociatedUserValidator::BlockDenyAccessUpdate() {
  base::AutoLock locker(validator_lock_);
  ++block_deny_access_update_;
}

void AssociatedUserValidator::UnblockDenyAccessUpdate() {
  base::AutoLock locker(validator_lock_);
  DCHECK(block_deny_access_update_ > 0);
  --block_deny_access_update_;
}

bool AssociatedUserValidator::IsDenyAccessUpdateBlocked() const {
  base::AutoLock locker(validator_lock_);
  return block_deny_access_update_ > 0;
}

bool AssociatedUserValidator::IsUserAccessBlockedForTesting(
    const base::string16& sid) const {
  base::AutoLock locker(validator_lock_);
  return locked_user_sids_.find(sid) != locked_user_sids_.end();
}

void AssociatedUserValidator::ForceRefreshTokenHandlesForTesting() {
  base::AutoLock locker(validator_lock_);
  for (const auto& user_info : user_to_token_handle_info_) {
    // Make the last update time outside the validity lifetime of the token
    // handle.
    user_info.second->last_update =
        base::Time::Now() - kTokenHandleValidityLifetime;
  }
}

}  // namespace credential_provider
