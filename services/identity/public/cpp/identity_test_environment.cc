// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_test_environment.h"

#include "build/build_config.h"

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "services/identity/public/cpp/primary_account_mutator.h"

#if !defined(OS_CHROMEOS)
#include "services/identity/public/cpp/primary_account_mutator_impl.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "services/identity/public/cpp/accounts_mutator_impl.h"
#endif

namespace identity {

class IdentityManagerDependenciesOwner {
 public:
  IdentityManagerDependenciesOwner(
      network::TestURLLoaderFactory* test_url_loader_factory,
      sync_preferences::TestingPrefServiceSyncable* pref_service,
      signin::AccountConsistencyMethod account_consistency);
  ~IdentityManagerDependenciesOwner();

  AccountTrackerService* account_tracker_service();

  SigninManagerForTest* signin_manager();

  FakeProfileOAuth2TokenService* token_service();

  FakeGaiaCookieManagerService* gaia_cookie_manager_service();

  sync_preferences::TestingPrefServiceSyncable* pref_service();

 private:
  // Depending on whether a |pref_service| instance is passed in
  // the constructor, exactly one of these will be non-null.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      owned_pref_service_;
  sync_preferences::TestingPrefServiceSyncable* raw_pref_service_ = nullptr;

  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  SigninManagerForTest signin_manager_;
  std::unique_ptr<FakeGaiaCookieManagerService> gaia_cookie_manager_service_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerDependenciesOwner);
};

IdentityManagerDependenciesOwner::IdentityManagerDependenciesOwner(
    network::TestURLLoaderFactory* test_url_loader_factory,
    sync_preferences::TestingPrefServiceSyncable* pref_service_param,
    signin::AccountConsistencyMethod account_consistency)
    : owned_pref_service_(
          pref_service_param
              ? nullptr
              : std::make_unique<
                    sync_preferences::TestingPrefServiceSyncable>()),
      raw_pref_service_(pref_service_param),
      signin_client_(pref_service()),
      token_service_(pref_service()),
#if defined(OS_CHROMEOS)
      signin_manager_(&signin_client_, &token_service_, &account_tracker_) {
#else
      signin_manager_(&signin_client_,
                      &token_service_,
                      &account_tracker_,
                      nullptr,
                      account_consistency) {
#endif
  if (test_url_loader_factory != nullptr) {
    gaia_cookie_manager_service_ =
        std::make_unique<FakeGaiaCookieManagerService>(
            &token_service_, &signin_client_, test_url_loader_factory);
  } else {
    gaia_cookie_manager_service_ =
        std::make_unique<FakeGaiaCookieManagerService>(&token_service_,
                                                       &signin_client_);
  }
  AccountTrackerService::RegisterPrefs(pref_service()->registry());
  ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service()->registry());
  SigninManagerBase::RegisterProfilePrefs(pref_service()->registry());
  SigninManagerBase::RegisterPrefs(pref_service()->registry());

  account_tracker_.Initialize(pref_service(), base::FilePath());
  signin_manager_.Initialize(pref_service());
}

IdentityManagerDependenciesOwner::~IdentityManagerDependenciesOwner() {}

AccountTrackerService*
IdentityManagerDependenciesOwner::account_tracker_service() {
  return &account_tracker_;
}

SigninManagerForTest* IdentityManagerDependenciesOwner::signin_manager() {
  return &signin_manager_;
}

FakeProfileOAuth2TokenService*
IdentityManagerDependenciesOwner::token_service() {
  return &token_service_;
}

FakeGaiaCookieManagerService*
IdentityManagerDependenciesOwner::gaia_cookie_manager_service() {
  return gaia_cookie_manager_service_.get();
}

sync_preferences::TestingPrefServiceSyncable*
IdentityManagerDependenciesOwner::pref_service() {
  DCHECK(raw_pref_service_ || owned_pref_service_);
  DCHECK(!(raw_pref_service_ && owned_pref_service_));

  return raw_pref_service_ ? raw_pref_service_ : owned_pref_service_.get();
}

IdentityTestEnvironment::IdentityTestEnvironment(
    network::TestURLLoaderFactory* test_url_loader_factory,
    sync_preferences::TestingPrefServiceSyncable* pref_service,
    signin::AccountConsistencyMethod account_consistency)
    : IdentityTestEnvironment(
          /*account_tracker_service=*/nullptr,
          /*token_service=*/nullptr,
          /*signin_manager=*/nullptr,
          /*gaia_cookie_manager_service=*/nullptr,
          std::make_unique<IdentityManagerDependenciesOwner>(
              test_url_loader_factory,
              pref_service,
              account_consistency),
          /*identity_manager=*/nullptr) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    AccountTrackerService* account_tracker_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerForTest* signin_manager,
    FakeGaiaCookieManagerService* gaia_cookie_manager_service)
    : IdentityTestEnvironment(account_tracker_service,
                              token_service,
                              signin_manager,
                              gaia_cookie_manager_service,
                              /*dependency_owner=*/nullptr,
                              /*identity_manager=*/nullptr) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    AccountTrackerService* account_tracker_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerForTest* signin_manager,
    FakeGaiaCookieManagerService* gaia_cookie_manager_service,
    IdentityManager* identity_manager)
    : IdentityTestEnvironment(account_tracker_service,
                              token_service,
                              signin_manager,
                              gaia_cookie_manager_service,
                              /*dependency_owner=*/nullptr,
                              identity_manager) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    AccountTrackerService* account_tracker_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerForTest* signin_manager,
    FakeGaiaCookieManagerService* gaia_cookie_manager_service,
    std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner,
    IdentityManager* identity_manager)
    : weak_ptr_factory_(this) {
  DCHECK(base::ThreadTaskRunnerHandle::Get())
      << "IdentityTestEnvironment requires a properly set up task environment. "
         "If your test has an existing one, move it to be initialized before "
         "IdentityTestEnvironment. Otherwise, use "
         "base::test::ScopedTaskEnvironment.";

  if (dependencies_owner) {
    DCHECK(!(account_tracker_service || token_service || signin_manager ||
             gaia_cookie_manager_service || identity_manager));

    dependencies_owner_ = std::move(dependencies_owner);

    account_tracker_service_ = dependencies_owner_->account_tracker_service();
    token_service_ = dependencies_owner_->token_service();
    signin_manager_ = dependencies_owner_->signin_manager();
    gaia_cookie_manager_service_ =
        dependencies_owner_->gaia_cookie_manager_service();

  } else {
    DCHECK(account_tracker_service && token_service && signin_manager &&
           gaia_cookie_manager_service);

    account_tracker_service_ = account_tracker_service;
    token_service_ = token_service;
    signin_manager_ = signin_manager;
    gaia_cookie_manager_service_ = gaia_cookie_manager_service;
  }

  if (identity_manager) {
    raw_identity_manager_ = identity_manager;
  } else {
    std::unique_ptr<PrimaryAccountMutator> primary_account_mutator;
    std::unique_ptr<AccountsMutator> accounts_mutator;
#if !defined(OS_CHROMEOS)
    primary_account_mutator = std::make_unique<PrimaryAccountMutatorImpl>(
        account_tracker_service_, static_cast<SigninManager*>(signin_manager_));
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
    accounts_mutator = std::make_unique<AccountsMutatorImpl>(
        token_service_, account_tracker_service_, signin_manager_);
#endif

    owned_identity_manager_ = std::make_unique<IdentityManager>(
        signin_manager_, token_service_, account_tracker_service_,
        gaia_cookie_manager_service_, std::move(primary_account_mutator),
        std::move(accounts_mutator));
  }

  this->identity_manager()->AddDiagnosticsObserver(this);
}

IdentityTestEnvironment::~IdentityTestEnvironment() {
  identity_manager()->RemoveDiagnosticsObserver(this);
}

IdentityManager* IdentityTestEnvironment::identity_manager() {
  DCHECK(raw_identity_manager_ || owned_identity_manager_);
  DCHECK(!(raw_identity_manager_ && owned_identity_manager_));

  return raw_identity_manager_ ? raw_identity_manager_
                               : owned_identity_manager_.get();
}

AccountInfo IdentityTestEnvironment::SetPrimaryAccount(
    const std::string& email) {
  return identity::SetPrimaryAccount(identity_manager(), email);
}

void IdentityTestEnvironment::SetRefreshTokenForPrimaryAccount() {
  identity::SetRefreshTokenForPrimaryAccount(identity_manager());
}

void IdentityTestEnvironment::SetInvalidRefreshTokenForPrimaryAccount() {
  identity::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());
}

void IdentityTestEnvironment::RemoveRefreshTokenForPrimaryAccount() {
  identity::RemoveRefreshTokenForPrimaryAccount(identity_manager());
}

AccountInfo IdentityTestEnvironment::MakePrimaryAccountAvailable(
    const std::string& email) {
  return identity::MakePrimaryAccountAvailable(identity_manager(), email);
}

void IdentityTestEnvironment::ClearPrimaryAccount(
    ClearPrimaryAccountPolicy policy) {
  identity::ClearPrimaryAccount(identity_manager(), policy);
}

AccountInfo IdentityTestEnvironment::MakeAccountAvailable(
    const std::string& email) {
  return identity::MakeAccountAvailable(identity_manager(), email);
}

void IdentityTestEnvironment::SetRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::SetRefreshTokenForAccount(identity_manager(), account_id);
}

void IdentityTestEnvironment::SetInvalidRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::SetInvalidRefreshTokenForAccount(identity_manager(),
                                                    account_id);
}

void IdentityTestEnvironment::RemoveRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::RemoveRefreshTokenForAccount(identity_manager(), account_id);
}

void IdentityTestEnvironment::UpdatePersistentErrorOfRefreshTokenForAccount(
    const std::string& account_id,
    const GoogleServiceAuthError& auth_error) {
  return identity::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), account_id, auth_error);
}

void IdentityTestEnvironment::SetCookieAccounts(
    const std::vector<CookieParams>& cookie_accounts) {
  identity::SetCookieAccounts(gaia_cookie_manager_service_, identity_manager(),
                              cookie_accounts);
}

void IdentityTestEnvironment::SetAutomaticIssueOfAccessTokens(bool grant) {
  token_service_->set_auto_post_fetch_response_on_message_loop(grant);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  token_service_->IssueTokenForAllPendingRequests(
      OAuth2AccessTokenConsumer::TokenResponse(token, expiration, id_token));
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const std::string& account_id,
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token) {
  WaitForAccessTokenRequestIfNecessary(account_id);
  token_service_->IssueAllTokensForAccount(
      account_id,
      OAuth2AccessTokenConsumer::TokenResponse(token, expiration, id_token));
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token,
        const identity::ScopeSet& scopes) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  token_service_->IssueTokenForScope(
      scopes,
      OAuth2AccessTokenConsumer::TokenResponse(token, expiration, id_token));
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        const GoogleServiceAuthError& error) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  token_service_->IssueErrorForAllPendingRequests(error);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        const std::string& account_id,
        const GoogleServiceAuthError& error) {
  WaitForAccessTokenRequestIfNecessary(account_id);
  token_service_->IssueErrorForAllPendingRequestsForAccount(account_id, error);
}

void IdentityTestEnvironment::SetCallbackForNextAccessTokenRequest(
    base::OnceClosure callback) {
  on_access_token_requested_callback_ = std::move(callback);
}

IdentityTestEnvironment::AccessTokenRequestState::AccessTokenRequestState() =
    default;
IdentityTestEnvironment::AccessTokenRequestState::~AccessTokenRequestState() =
    default;
IdentityTestEnvironment::AccessTokenRequestState::AccessTokenRequestState(
    AccessTokenRequestState&& other) = default;
IdentityTestEnvironment::AccessTokenRequestState&
IdentityTestEnvironment::AccessTokenRequestState::operator=(
    AccessTokenRequestState&& other) = default;

void IdentityTestEnvironment::OnAccessTokenRequested(
    const std::string& account_id,
    const std::string& consumer_id,
    const identity::ScopeSet& scopes) {
  // Post a task to handle this access token request in order to support the
  // case where the access token request is handled synchronously in the
  // production code, in which case this callback could be coming in ahead
  // of an invocation of WaitForAccessTokenRequestIfNecessary() that will be
  // made in this same iteration of the run loop.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&IdentityTestEnvironment::HandleOnAccessTokenRequested,
                     weak_ptr_factory_.GetWeakPtr(), account_id));
}

void IdentityTestEnvironment::HandleOnAccessTokenRequested(
    std::string account_id) {
  if (on_access_token_requested_callback_) {
    std::move(on_access_token_requested_callback_).Run();
    return;
  }

  for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
    if (!it->account_id || (it->account_id.value() == account_id)) {
      if (it->state == AccessTokenRequestState::kAvailable)
        return;
      if (it->on_available)
        std::move(it->on_available).Run();
      requesters_.erase(it);
      return;
    }
  }

  // A requests came in for a request for which we are not waiting. Record that
  // it's available.
  requesters_.emplace_back();
  requesters_.back().state = AccessTokenRequestState::kAvailable;
  requesters_.back().account_id = account_id;
}

void IdentityTestEnvironment::WaitForAccessTokenRequestIfNecessary(
    base::Optional<std::string> account_id) {
  // Handle HandleOnAccessTokenRequested getting called before
  // WaitForAccessTokenRequestIfNecessary.
  if (account_id) {
    for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
      if (it->account_id && it->account_id.value() == account_id.value()) {
        // Can't wait twice for same thing.
        DCHECK_EQ(AccessTokenRequestState::kAvailable, it->state);
        requesters_.erase(it);
        return;
      }
    }
  } else {
    for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
      if (it->state == AccessTokenRequestState::kAvailable) {
        requesters_.erase(it);
        return;
      }
    }
  }

  base::RunLoop run_loop;
  requesters_.emplace_back();
  requesters_.back().state = AccessTokenRequestState::kPending;
  requesters_.back().account_id = std::move(account_id);
  requesters_.back().on_available = run_loop.QuitClosure();
  run_loop.Run();
}

void IdentityTestEnvironment::UpdateAccountInfoForAccount(
    AccountInfo account_info) {
  identity::UpdateAccountInfoForAccount(identity_manager(), account_info);
}

void IdentityTestEnvironment::ResetToAccountsNotYetLoadedFromDiskState() {
  token_service_->set_all_credentials_loaded_for_testing(false);
}

void IdentityTestEnvironment::ReloadAccountsFromDisk() {
  token_service_->LoadCredentials("");
}

void IdentityTestEnvironment::SetFreshnessOfAccountsInGaiaCookie(
    bool accounts_are_fresh) {
  gaia_cookie_manager_service_->set_list_accounts_stale_for_testing(
      accounts_are_fresh);
}

}  // namespace identity
