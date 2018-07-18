// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/identity_test_environment.h"

#include "base/run_loop.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/identity/public/cpp/identity_test_utils.h"

#if defined(OS_CHROMEOS)
using SigninManagerForTest = FakeSigninManagerBase;
#else
using SigninManagerForTest = FakeSigninManager;
#endif  // OS_CHROMEOS

namespace identity {

// Internal class that abstracts the dependencies out of the public interface.
class IdentityTestEnvironmentInternal {
 public:
  IdentityTestEnvironmentInternal();
  ~IdentityTestEnvironmentInternal();

  // The IdentityManager instance created and owned by this instance.
  IdentityManager* identity_manager();

  AccountTrackerService* account_tracker_service();

  SigninManagerForTest* signin_manager();

  FakeProfileOAuth2TokenService* token_service();

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  SigninManagerForTest signin_manager_;
  FakeProfileOAuth2TokenService token_service_;
  FakeGaiaCookieManagerService gaia_cookie_manager_service_;
  std::unique_ptr<IdentityManager> identity_manager_;

  DISALLOW_COPY_AND_ASSIGN(IdentityTestEnvironmentInternal);
};

IdentityTestEnvironmentInternal::IdentityTestEnvironmentInternal()
    : signin_client_(&pref_service_),
#if defined(OS_CHROMEOS)
      signin_manager_(&signin_client_, &account_tracker_),
#else
      signin_manager_(&signin_client_,
                      &token_service_,
                      &account_tracker_,
                      nullptr),
#endif
      // NOTE: Some unittests set up their own TestURLFetcherFactory. In these
      // contexts FakeGaiaCookieManagerService can't set up its own
      // FakeURLFetcherFactory, as {Test, Fake}URLFetcherFactory allow only one
      // instance to be alive at a time. If some users of
      // IdentityTestEnvironment require that GaiaCookieManagerService have a
      // FakeURLFetcherFactory, we'll need to pass a config param in to
      // IdentityTestEnvironment to specify this. If some users want that
      // behavior while *also* having their own FakeURLFetcherFactory, we'll
      // need to pass the actual object in and have GaiaCookieManagerService
      // have a reference to the object (or figure out the sharing some other
      // way). Contact blundell@chromium.org if you come up against this issue.
      gaia_cookie_manager_service_(&token_service_,
                                   "identity_test_environment",
                                   &signin_client_,
                                   /*use_fake_url_fetcher=*/false) {
  AccountTrackerService::RegisterPrefs(pref_service_.registry());
  SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
  SigninManagerBase::RegisterPrefs(pref_service_.registry());

  account_tracker_.Initialize(&signin_client_);

  identity_manager_.reset(new IdentityManager(&signin_manager_, &token_service_,
                                              &account_tracker_,
                                              &gaia_cookie_manager_service_));
}

IdentityTestEnvironmentInternal::~IdentityTestEnvironmentInternal() {}

IdentityManager* IdentityTestEnvironmentInternal::identity_manager() {
  return identity_manager_.get();
}

AccountTrackerService*
IdentityTestEnvironmentInternal::account_tracker_service() {
  return &account_tracker_;
}

SigninManagerForTest* IdentityTestEnvironmentInternal::signin_manager() {
  return &signin_manager_;
}

FakeProfileOAuth2TokenService*
IdentityTestEnvironmentInternal::token_service() {
  return &token_service_;
}

IdentityTestEnvironment::IdentityTestEnvironment()
    : internals_(std::make_unique<IdentityTestEnvironmentInternal>()) {
  internals_->identity_manager()->AddDiagnosticsObserver(this);
}

IdentityTestEnvironment::~IdentityTestEnvironment() {
  internals_->identity_manager()->RemoveDiagnosticsObserver(this);
}

IdentityManager* IdentityTestEnvironment::identity_manager() {
  return internals_->identity_manager();
}

AccountInfo IdentityTestEnvironment::SetPrimaryAccount(
    const std::string& email) {
  return identity::SetPrimaryAccount(internals_->signin_manager(),
                                     internals_->identity_manager(), email);
}

void IdentityTestEnvironment::SetRefreshTokenForPrimaryAccount() {
  identity::SetRefreshTokenForPrimaryAccount(internals_->token_service(),
                                             internals_->identity_manager());
}

void IdentityTestEnvironment::SetInvalidRefreshTokenForPrimaryAccount() {
  identity::SetInvalidRefreshTokenForPrimaryAccount(
      internals_->token_service(), internals_->identity_manager());
}

void IdentityTestEnvironment::RemoveRefreshTokenForPrimaryAccount() {
  identity::RemoveRefreshTokenForPrimaryAccount(internals_->token_service(),
                                                internals_->identity_manager());
}

AccountInfo IdentityTestEnvironment::MakePrimaryAccountAvailable(
    const std::string& email) {
  return identity::MakePrimaryAccountAvailable(
      internals_->signin_manager(), internals_->token_service(),
      internals_->identity_manager(), email);
}

void IdentityTestEnvironment::ClearPrimaryAccount() {
  identity::ClearPrimaryAccount(internals_->signin_manager(),
                                internals_->identity_manager());
}

AccountInfo IdentityTestEnvironment::MakeAccountAvailable(
    const std::string& email) {
  return identity::MakeAccountAvailable(internals_->account_tracker_service(),
                                        internals_->token_service(),
                                        internals_->identity_manager(), email);
}

void IdentityTestEnvironment::SetRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::SetRefreshTokenForAccount(
      internals_->token_service(), internals_->identity_manager(), account_id);
}

void IdentityTestEnvironment::SetInvalidRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::SetInvalidRefreshTokenForAccount(
      internals_->token_service(), internals_->identity_manager(), account_id);
}

void IdentityTestEnvironment::RemoveRefreshTokenForAccount(
    const std::string& account_id) {
  return identity::RemoveRefreshTokenForAccount(
      internals_->token_service(), internals_->identity_manager(), account_id);
}

void IdentityTestEnvironment::SetAutomaticIssueOfAccessTokens(bool grant) {
  internals_->token_service()->set_auto_post_fetch_response_on_message_loop(
      grant);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const std::string& token,
        const base::Time& expiration) {
  WaitForAccessTokenRequestIfNecessary();
  internals_->token_service()->IssueTokenForAllPendingRequests(token,
                                                               expiration);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        const GoogleServiceAuthError& error) {
  WaitForAccessTokenRequestIfNecessary();
  internals_->token_service()->IssueErrorForAllPendingRequests(error);
}

void IdentityTestEnvironment::SetCallbackForNextAccessTokenRequest(
    base::OnceClosure callback) {
  on_access_token_requested_callback_ = std::move(callback);
}

void IdentityTestEnvironment::OnAccessTokenRequested(
    const std::string& account_id,
    const std::string& consumer_id,
    const OAuth2TokenService::ScopeSet& scopes) {
  // Post a task to handle this access token request in order to support the
  // case where the access token request is handled synchronously in the
  // production code, in which case this callback could be coming in ahead
  // of an invocation of WaitForAccessTokenRequestIfNecessary() that will be
  // made in this same iteration of the run loop.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&IdentityTestEnvironment::HandleOnAccessTokenRequested,
                     base::Unretained(this)));
}

void IdentityTestEnvironment::HandleOnAccessTokenRequested() {
  if (on_access_token_requested_callback_)
    std::move(on_access_token_requested_callback_).Run();
}

void IdentityTestEnvironment::WaitForAccessTokenRequestIfNecessary() {
  DCHECK(!on_access_token_requested_callback_);
  base::RunLoop run_loop;
  on_access_token_requested_callback_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace identity
