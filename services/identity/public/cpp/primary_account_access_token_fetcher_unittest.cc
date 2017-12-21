// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MockCallback;
using sync_preferences::TestingPrefServiceSyncable;
using testing::CallbackToFunctor;
using testing::InvokeWithoutArgs;
using testing::StrictMock;

#if defined(OS_CHROMEOS)
// ChromeOS doesn't have SigninManager.
using SigninManagerForTest = FakeSigninManagerBase;
#else
using SigninManagerForTest = FakeSigninManager;
#endif  // OS_CHROMEOS

namespace identity {

class PrimaryAccountAccessTokenFetcherTest : public testing::Test {
 public:
  using TestTokenCallback =
      StrictMock<MockCallback<PrimaryAccountAccessTokenFetcher::TokenCallback>>;

  PrimaryAccountAccessTokenFetcherTest() : signin_client_(&pref_service_) {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    signin::RegisterAccountConsistencyProfilePrefs(pref_service_.registry());
#if defined(OS_CHROMEOS)
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());
#else
    SigninManager::RegisterProfilePrefs(pref_service_.registry());
    SigninManager::RegisterPrefs(pref_service_.registry());
#endif  // OS_CHROMEOS

    signin::SetGaiaOriginIsolatedCallback(
        base::BindRepeating([] { return true; }));

    account_tracker_ = base::MakeUnique<AccountTrackerService>();
    account_tracker_->Initialize(&signin_client_);

#if defined(OS_CHROMEOS)
    signin_manager_ = base::MakeUnique<FakeSigninManagerBase>(
        &signin_client_, account_tracker_.get());
#else
    signin_manager_ = base::MakeUnique<FakeSigninManager>(
        &signin_client_, &token_service_, account_tracker_.get(),
        /*cookie_manager_service=*/nullptr);
#endif  // OS_CHROMEOS
  }

  ~PrimaryAccountAccessTokenFetcherTest() override {}

  std::unique_ptr<PrimaryAccountAccessTokenFetcher> CreateFetcher(
      PrimaryAccountAccessTokenFetcher::TokenCallback callback) {
    std::set<std::string> scopes{"scope"};
    return base::MakeUnique<PrimaryAccountAccessTokenFetcher>(
        "test_consumer", signin_manager_.get(), &token_service_, scopes,
        std::move(callback));
  }

  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }
  SigninManagerForTest* signin_manager() { return signin_manager_.get(); }

  void SignIn(const std::string& account) {
#if defined(OS_CHROMEOS)
    signin_manager_->SignIn(account);
#else
    signin_manager_->SignIn(account, "user", "pass");
#endif  // OS_CHROMEOS
  }

 private:
  TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;

  std::unique_ptr<AccountTrackerService> account_tracker_;
  std::unique_ptr<SigninManagerForTest> signin_manager_;
};

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldReturnAccessToken) {
  TestTokenCallback callback;

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(callback.Get());

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      "account", "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldNotReplyIfDestroyed) {
  TestTokenCallback callback;

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(callback.Get());

  // Destroy the fetcher before the access token request is fulfilled.
  fetcher.reset();

  // Now fulfilling the access token request should have no effect.
  token_service()->IssueAllTokensForAccount(
      "account", "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldNotReturnWhenSignedOut) {
  TestTokenCallback callback;

  // Signed out -> the fetcher should wait for a sign-in which never happens
  // in this test, so we shouldn't get called back.
  auto fetcher = CreateFetcher(callback.Get());
}

// Tests related to waiting for sign-in don't apply on ChromeOS (it doesn't have
// that concept).
#if !defined(OS_CHROMEOS)

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldWaitForSignIn) {
  TestTokenCallback callback;

  // Not signed in, so this should wait for a sign-in to complete.
  auto fetcher = CreateFetcher(callback.Get());

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      "account", "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldWaitForSignInInProgress) {
  TestTokenCallback callback;

  signin_manager()->set_auth_in_progress("account");

  // A sign-in is currently in progress, so this should wait for the sign-in to
  // complete.
  auto fetcher = CreateFetcher(callback.Get());

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      "account", "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldWaitForFailedSignIn) {
  TestTokenCallback callback;

  signin_manager()->set_auth_in_progress("account");

  // A sign-in is currently in progress, so this should wait for the sign-in to
  // complete.
  auto fetcher = CreateFetcher(callback.Get());

  // The fetcher should detect the failed sign-in and call us with an empty
  // access token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
          std::string()));

  signin_manager()->FailSignin(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
}

#endif  // !OS_CHROMEOS

TEST_F(PrimaryAccountAccessTokenFetcherTest, ShouldWaitForRefreshToken) {
  TestTokenCallback callback;

  SignIn("account");

  // Signed in, but there is no refresh token -> we should not get called back
  // (yet).
  auto fetcher = CreateFetcher(callback.Get());

  // Getting a refresh token should result in a request for an access token.
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      "account", "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldIgnoreRefreshTokensForOtherAccounts) {
  TestTokenCallback callback;

  // Signed-in to "account", but there's only a refresh token for a different
  // account.
  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account 2", "refresh");

  // The fetcher should wait for the correct refresh token.
  auto fetcher = CreateFetcher(callback.Get());

  // A refresh token for yet another account shouldn't matter either.
  token_service()->GetDelegate()->UpdateCredentials("account 3", "refresh");
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldReturnWhenNoRefreshTokenAvailable) {
  TestTokenCallback callback;

  SignIn("account");

  // Signed in, but there is no refresh token -> we should not get called back
  // (yet).
  auto fetcher = CreateFetcher(callback.Get());

  // Getting a refresh token for some other account should have no effect.
  token_service()->GetDelegate()->UpdateCredentials("different account",
                                                    "refresh token");

  // The OAuth2TokenService posts a task to the current thread when we try to
  // get an access token for an account without a refresh token, so we need a
  // MessageLoop in this test to not crash.
  base::MessageLoop message_loop;

  // When all refresh tokens have been loaded by the token service, but the one
  // for our account wasn't among them, we should get called back with an empty
  // access token.
  EXPECT_CALL(callback, Run(testing::_, std::string()));
  token_service()->GetDelegate()->LoadCredentials("account doesn't matter");

  // Wait for the task posted by OAuth2TokenService to run.
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldRetryCanceledAccessTokenRequest) {
  TestTokenCallback callback;

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(callback.Get());

  // A canceled access token request should get retried once.
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      "account",
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // Once the access token request is fulfilled, we should get called back with
  // the access token.
  EXPECT_CALL(callback,
              Run(GoogleServiceAuthError::AuthErrorNone(), "access token"));
  token_service()->IssueAllTokensForAccount(
      "account", "access token",
      base::Time::Now() + base::TimeDelta::FromHours(1));
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldRetryCanceledAccessTokenRequestOnlyOnce) {
  TestTokenCallback callback;

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(callback.Get());

  // A canceled access token request should get retried once.
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      "account",
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // On the second failure, we should get called back with an empty access
  // token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          std::string()));
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      "account",
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

#if !defined(OS_CHROMEOS)

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryCanceledAccessTokenRequestIfSignedOut) {
  TestTokenCallback callback;

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(callback.Get());

  // Simulate the user signing out while the access token request is pending.
  // In this case, the pending request gets canceled, and the fetcher should
  // *not* retry.
  signin_manager()->ForceSignOut();
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          std::string()));
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      "account",
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

#endif  // !OS_CHROMEOS

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryCanceledAccessTokenRequestIfRefreshTokenRevoked) {
  TestTokenCallback callback;

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(callback.Get());

  // Simulate the refresh token getting invalidated. In this case, pending
  // access token requests get canceled, and the fetcher should *not* retry.
  token_service()->GetDelegate()->RevokeCredentials("account");
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED),
          std::string()));
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      "account",
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

TEST_F(PrimaryAccountAccessTokenFetcherTest,
       ShouldNotRetryFailedAccessTokenRequest) {
  TestTokenCallback callback;

  SignIn("account");
  token_service()->GetDelegate()->UpdateCredentials("account", "refresh token");

  // Signed in and refresh token already exists, so this should result in a
  // request for an access token.
  auto fetcher = CreateFetcher(callback.Get());

  // An access token failure other than "canceled" should not be retried; we
  // should immediately get called back with an empty access token.
  EXPECT_CALL(
      callback,
      Run(GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE),
          std::string()));
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      "account",
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

}  // namespace identity
