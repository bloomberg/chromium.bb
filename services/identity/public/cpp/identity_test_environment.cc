// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_test_environment.h"

#include "base/bind.h"
#include "build/build_config.h"

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/identity/public/cpp/accounts_cookie_mutator.h"
#include "services/identity/public/cpp/accounts_cookie_mutator_impl.h"
#include "services/identity/public/cpp/accounts_mutator.h"
#include "services/identity/public/cpp/diagnostics_provider_impl.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "services/identity/public/cpp/primary_account_mutator.h"
#include "services/identity/public/cpp/test_identity_manager_observer.h"

#if !defined(OS_CHROMEOS)
#include "services/identity/public/cpp/primary_account_mutator_impl.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "services/identity/public/cpp/accounts_mutator_impl.h"
#endif

#if defined(OS_ANDROID)
#include "components/signin/core/browser/child_account_info_fetcher_android.h"
#endif

namespace identity {

class IdentityManagerDependenciesOwner {
 public:
  IdentityManagerDependenciesOwner(
      network::TestURLLoaderFactory* test_url_loader_factory,
      sync_preferences::TestingPrefServiceSyncable* pref_service,
      signin::AccountConsistencyMethod account_consistency,
      TestSigninClient* test_signin_client);
  ~IdentityManagerDependenciesOwner();

  AccountTrackerService* account_tracker_service();

  AccountFetcherService* account_fetcher_service();

  SigninManagerBase* signin_manager();

  FakeProfileOAuth2TokenService* token_service();

  GaiaCookieManagerService* gaia_cookie_manager_service();

  sync_preferences::TestingPrefServiceSyncable* pref_service();

  TestSigninClient* signin_client();

 private:
  // Depending on whether a |pref_service| instance is passed in
  // the constructor, exactly one of these will be non-null.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      owned_pref_service_;
  sync_preferences::TestingPrefServiceSyncable* raw_pref_service_ = nullptr;

  std::unique_ptr<TestSigninClient> owned_signin_client_;
  TestSigninClient* raw_signin_client_ = nullptr;

  AccountTrackerService account_tracker_;
  AccountFetcherService account_fetcher_;
  FakeProfileOAuth2TokenService token_service_;
#if defined(OS_CHROMEOS)
  SigninManagerBase signin_manager_;
#else
  SigninManager signin_manager_;
#endif
  std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service_;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerDependenciesOwner);
};

IdentityManagerDependenciesOwner::IdentityManagerDependenciesOwner(
    network::TestURLLoaderFactory* test_url_loader_factory,
    sync_preferences::TestingPrefServiceSyncable* pref_service_param,
    signin::AccountConsistencyMethod account_consistency,
    TestSigninClient* signin_client_param)
    : owned_pref_service_(
          pref_service_param
              ? nullptr
              : std::make_unique<
                    sync_preferences::TestingPrefServiceSyncable>()),
      raw_pref_service_(pref_service_param),
      owned_signin_client_(
          signin_client_param
              ? nullptr
              : std::make_unique<TestSigninClient>(pref_service())),
      raw_signin_client_(signin_client_param),
      token_service_(pref_service()),
#if defined(OS_CHROMEOS)
      signin_manager_(signin_client(), &token_service_, &account_tracker_) {
#else
      signin_manager_(signin_client(),
                      &token_service_,
                      &account_tracker_,
                      nullptr,
                      account_consistency) {
#endif
  if (test_url_loader_factory != nullptr) {
    gaia_cookie_manager_service_ = std::make_unique<GaiaCookieManagerService>(
        &token_service_, signin_client(),
        base::BindRepeating(
            [](network::TestURLLoaderFactory* test_url_loader_factory)
                -> scoped_refptr<network::SharedURLLoaderFactory> {
              return test_url_loader_factory->GetSafeWeakWrapper();
            },
            test_url_loader_factory));
  } else {
    gaia_cookie_manager_service_ = std::make_unique<GaiaCookieManagerService>(
        &token_service_, signin_client());
  }
  AccountTrackerService::RegisterPrefs(pref_service()->registry());
  AccountFetcherService::RegisterPrefs(pref_service()->registry());
  ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service()->registry());
  SigninManagerBase::RegisterProfilePrefs(pref_service()->registry());
  SigninManagerBase::RegisterPrefs(pref_service()->registry());

  account_tracker_.Initialize(pref_service(), base::FilePath());
  account_fetcher_.Initialize(
      signin_client(), &token_service_, &account_tracker_,
      std::make_unique<image_fetcher::FakeImageDecoder>());
  signin_manager_.Initialize(pref_service());
}

IdentityManagerDependenciesOwner::~IdentityManagerDependenciesOwner() {
  signin_manager_.Shutdown();
  account_fetcher_.Shutdown();
  account_tracker_.Shutdown();
}

AccountTrackerService*
IdentityManagerDependenciesOwner::account_tracker_service() {
  return &account_tracker_;
}

AccountFetcherService*
IdentityManagerDependenciesOwner::account_fetcher_service() {
  return &account_fetcher_;
}

SigninManagerBase* IdentityManagerDependenciesOwner::signin_manager() {
  return &signin_manager_;
}

FakeProfileOAuth2TokenService*
IdentityManagerDependenciesOwner::token_service() {
  return &token_service_;
}

GaiaCookieManagerService*
IdentityManagerDependenciesOwner::gaia_cookie_manager_service() {
  return gaia_cookie_manager_service_.get();
}

sync_preferences::TestingPrefServiceSyncable*
IdentityManagerDependenciesOwner::pref_service() {
  DCHECK(raw_pref_service_ || owned_pref_service_);
  DCHECK(!(raw_pref_service_ && owned_pref_service_));

  return raw_pref_service_ ? raw_pref_service_ : owned_pref_service_.get();
}

TestSigninClient* IdentityManagerDependenciesOwner::signin_client() {
  DCHECK(raw_signin_client_ || owned_signin_client_);
  DCHECK(!(raw_signin_client_ && owned_signin_client_));

  return raw_signin_client_ ? raw_signin_client_ : owned_signin_client_.get();
}

IdentityTestEnvironment::IdentityTestEnvironment(
    network::TestURLLoaderFactory* test_url_loader_factory,
    sync_preferences::TestingPrefServiceSyncable* pref_service,
    signin::AccountConsistencyMethod account_consistency,
    TestSigninClient* test_signin_client)
    : IdentityTestEnvironment(
          /*pref_service=*/nullptr,
          /*account_tracker_service=*/nullptr,
          /*account_fetcher_service=*/nullptr,
          /*token_service=*/nullptr,
          /*signin_manager=*/nullptr,
          /*gaia_cookie_manager_service=*/nullptr,
          /*test_url_loader_factory=*/test_url_loader_factory,
          std::make_unique<IdentityManagerDependenciesOwner>(
              test_url_loader_factory,
              pref_service,
              account_consistency,
              test_signin_client),
          /*identity_manager=*/nullptr) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    PrefService* pref_service,
    AccountTrackerService* account_tracker_service,
    AccountFetcherService* account_fetcher_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    GaiaCookieManagerService* gaia_cookie_manager_service,
    network::TestURLLoaderFactory* test_url_loader_factory)
    : IdentityTestEnvironment(pref_service,
                              account_tracker_service,
                              account_fetcher_service,
                              token_service,
                              signin_manager,
                              gaia_cookie_manager_service,
                              test_url_loader_factory,
                              /*dependency_owner=*/nullptr,
                              /*identity_manager=*/nullptr) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    PrefService* pref_service,
    AccountTrackerService* account_tracker_service,
    AccountFetcherService* account_fetcher_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    GaiaCookieManagerService* gaia_cookie_manager_service,
    IdentityManager* identity_manager,
    network::TestURLLoaderFactory* test_url_loader_factory)
    : IdentityTestEnvironment(pref_service,
                              account_tracker_service,
                              account_fetcher_service,
                              token_service,
                              signin_manager,
                              gaia_cookie_manager_service,
                              test_url_loader_factory,
                              /*dependency_owner=*/nullptr,
                              identity_manager) {}

IdentityTestEnvironment::IdentityTestEnvironment(
    PrefService* pref_service,
    AccountTrackerService* account_tracker_service,
    AccountFetcherService* account_fetcher_service,
    FakeProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    GaiaCookieManagerService* gaia_cookie_manager_service,
    network::TestURLLoaderFactory* test_url_loader_factory,
    std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner,
    IdentityManager* identity_manager)
    : pref_service_(pref_service),
      test_url_loader_factory_(test_url_loader_factory),
      weak_ptr_factory_(this) {
  DCHECK(base::ThreadTaskRunnerHandle::Get())
      << "IdentityTestEnvironment requires a properly set up task environment. "
         "If your test has an existing one, move it to be initialized before "
         "IdentityTestEnvironment. Otherwise, use "
         "base::test::ScopedTaskEnvironment.";

  if (dependencies_owner) {
    DCHECK(!(pref_service_ || account_tracker_service ||
             account_fetcher_service || token_service || signin_manager ||
             gaia_cookie_manager_service || identity_manager));

    dependencies_owner_ = std::move(dependencies_owner);

    account_tracker_service_ = dependencies_owner_->account_tracker_service();
    account_fetcher_service_ = dependencies_owner_->account_fetcher_service();
    token_service_ = dependencies_owner_->token_service();
    signin_manager_ = dependencies_owner_->signin_manager();
    gaia_cookie_manager_service_ =
        dependencies_owner_->gaia_cookie_manager_service();
    pref_service_ = dependencies_owner_->pref_service();
  } else {
    DCHECK(pref_service_ && account_tracker_service &&
           account_fetcher_service && token_service && signin_manager &&
           gaia_cookie_manager_service);

    account_tracker_service_ = account_tracker_service;
    account_fetcher_service_ = account_fetcher_service;
    token_service_ = token_service;
    signin_manager_ = signin_manager;
    gaia_cookie_manager_service_ = gaia_cookie_manager_service;
  }

  // TODO(sdefresne): services should be initialized when this version of
  // the constructor is used. However, this break a large number of tests
  // (all those that use an IdentityTestEnvironment and its dependencies
  // as member fields; they should be changed to before the check can be
  // enabled).
  // DCHECK(account_tracker_service_->account_fetcher_service())
  //     << "IdentityTestEnvironment requires its services to be initialized "
  //     << "before passing them to the constructor.";

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
        token_service_, account_tracker_service_, signin_manager_,
        pref_service_);
#endif
    std::unique_ptr<DiagnosticsProvider> diagnostics_provider =
        std::make_unique<DiagnosticsProviderImpl>(token_service_,
                                                  gaia_cookie_manager_service_);

    std::unique_ptr<AccountsCookieMutator> accounts_cookie_mutator =
        std::make_unique<AccountsCookieMutatorImpl>(
            gaia_cookie_manager_service_);

    owned_identity_manager_ = std::make_unique<IdentityManager>(
        signin_manager_, token_service_, account_fetcher_service_,
        account_tracker_service_, gaia_cookie_manager_service_,
        std::move(primary_account_mutator), std::move(accounts_mutator),
        std::move(accounts_cookie_mutator), std::move(diagnostics_provider));
  }

  test_identity_manager_observer_ =
      std::make_unique<TestIdentityManagerObserver>(this->identity_manager());

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

TestIdentityManagerObserver*
IdentityTestEnvironment::identity_manager_observer() {
  return test_identity_manager_observer_.get();
}

CoreAccountInfo IdentityTestEnvironment::SetPrimaryAccount(
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
  DCHECK(test_url_loader_factory_)
      << "IdentityTestEnvironment constructor must have been passed a "
         "test_url_loader_factory in order to use this method.";
  identity::SetCookieAccounts(identity_manager(), test_url_loader_factory_,
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

bool IdentityTestEnvironment::IsAccessTokenRequestPending() {
  return token_service_->GetPendingRequests().size();
}

void IdentityTestEnvironment::SetFreshnessOfAccountsInGaiaCookie(
    bool accounts_are_fresh) {
  identity::SetFreshnessOfAccountsInGaiaCookie(identity_manager(),
                                               accounts_are_fresh);
}

void IdentityTestEnvironment::EnableRemovalOfExtendedAccountInfo() {
  account_fetcher_service_->EnableAccountRemovalForTest();
}

void IdentityTestEnvironment::SimulateSuccessfulFetchOfAccountInfo(
    const std::string& account_id,
    const std::string& email,
    const std::string& gaia,
    const std::string& hosted_domain,
    const std::string& full_name,
    const std::string& given_name,
    const std::string& locale,
    const std::string& picture_url) {
  identity::SimulateSuccessfulFetchOfAccountInfo(
      identity_manager(), account_id, email, gaia, hosted_domain, full_name,
      given_name, locale, picture_url);
}

void IdentityTestEnvironment::SimulateMergeSessionFailure(
    const GoogleServiceAuthError& auth_error) {
  // GaiaCookieManagerService changes the visibility of inherited method
  // OnMergeSessionFailure from public to private. Cast to a base class pointer
  // to use call the method.
  static_cast<GaiaAuthConsumer*>(gaia_cookie_manager_service_)
      ->OnMergeSessionFailure(auth_error);
}

}  // namespace identity
