// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/identity/identity_service.h"
#include "services/identity/public/cpp/account_state.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/identity/public/mojom/account.mojom.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace identity {
namespace {

const char kTestGaiaId[] = "dummyId";
const char kTestEmail[] = "me@dummy.com";
const char kTestRefreshToken[] = "dummy-refresh-token";
const char kTestAccessToken[] = "access_token";

class IdentityAccessorImplTest : public testing::Test {
 public:
  IdentityAccessorImplTest()
      : signin_client_(&pref_service_),
        token_service_(&pref_service_),
#if defined(OS_CHROMEOS)
        signin_manager_(&signin_client_, &token_service_, &account_tracker_),
#else
        signin_manager_(&signin_client_,
                        &token_service_,
                        &account_tracker_,
                        nullptr,
                        signin::AccountConsistencyMethod::kDisabled),
#endif
        gaia_cookie_manager_service_(
            &token_service_,
            &signin_client_,
            base::BindRepeating(
                [](network::TestURLLoaderFactory* test_url_loader_factory)
                    -> scoped_refptr<network::SharedURLLoaderFactory> {
                  return test_url_loader_factory->GetSafeWeakWrapper();
                },
                signin_client_.test_url_loader_factory())),
        identity_test_environment_(&pref_service_,
                                   &account_tracker_,
                                   &account_fetcher_,
                                   &token_service_,
                                   &signin_manager_,
                                   &gaia_cookie_manager_service_,
                                   signin_client_.test_url_loader_factory()),
        service_(
            identity_test_environment_.identity_manager(),
            &account_tracker_,
            &signin_manager_,
            &token_service_,
            test_connector_factory_.RegisterInstance(mojom::kServiceName)) {
    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    AccountFetcherService::RegisterPrefs(pref_service_.registry());
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());

    account_tracker_.Initialize(&pref_service_, base::FilePath());
    account_fetcher_.Initialize(&signin_client_, &token_service_,
                                &account_tracker_,
                                std::make_unique<TestImageDecoder>());
  }

  ~IdentityAccessorImplTest() override {
    token_service_.Shutdown();
    signin_client_.Shutdown();
    account_tracker_.Shutdown();
    gaia_cookie_manager_service_.Shutdown();
    account_fetcher_.Shutdown();
  }

  void TearDown() override {
    // Explicitly destruct IdentityAccessorImpl so that it doesn't outlive its
    // dependencies.
    ResetIdentityAccessorImpl();

    // Wait for the IdentityAccessorImpl to be notified of disconnection and
    // destroy itself.
    base::RunLoop().RunUntilIdle();
  }

  void OnReceivedPrimaryAccountInfo(
      base::RepeatingClosure quit_closure,
      const base::Optional<AccountInfo>& account_info,
      const AccountState& account_state) {
    primary_account_info_ = account_info;
    primary_account_state_ = account_state;
    quit_closure.Run();
  }

  void OnPrimaryAccountAvailable(base::RepeatingClosure quit_closure,
                                 AccountInfo* caller_account_info,
                                 AccountState* caller_account_state,
                                 const AccountInfo& account_info,
                                 const AccountState& account_state) {
    *caller_account_info = account_info;
    *caller_account_state = account_state;
    quit_closure.Run();
  }

  void OnReceivedAccountInfoFromGaiaId(
      base::RepeatingClosure quit_closure,
      const base::Optional<AccountInfo>& account_info,
      const AccountState& account_state) {
    account_info_from_gaia_id_ = account_info;
    account_state_from_gaia_id_ = account_state;
    quit_closure.Run();
  }

  void OnGotAccounts(base::RepeatingClosure quit_closure,
                     std::vector<mojom::AccountPtr>* output,
                     std::vector<mojom::AccountPtr> accounts) {
    *output = std::move(accounts);
    quit_closure.Run();
  }

  void OnReceivedAccessToken(base::RepeatingClosure quit_closure,
                             const base::Optional<std::string>& access_token,
                             base::Time expiration_time,
                             const GoogleServiceAuthError& error) {
    access_token_ = access_token;
    access_token_error_ = error;
    quit_closure.Run();
  }

 protected:
  mojom::IdentityAccessor* GetIdentityAccessorImpl() {
    if (!identity_accessor_) {
      test_connector_factory_.GetDefaultConnector()->BindInterface(
          mojom::kServiceName, &identity_accessor_);
    }
    return identity_accessor_.get();
  }

  void ResetIdentityAccessorImpl() { identity_accessor_.reset(); }

  void FlushIdentityAccessorImplForTesting() {
    GetIdentityAccessorImpl();
    identity_accessor_.FlushForTesting();
  }

  void SetIdentityAccessorImplConnectionErrorHandler(
      base::RepeatingClosure handler) {
    GetIdentityAccessorImpl();
    identity_accessor_.set_connection_error_handler(handler);
  }

  base::test::ScopedTaskEnvironment task_environemnt_;

  mojom::IdentityAccessorPtr identity_accessor_;
  base::Optional<AccountInfo> primary_account_info_;
  AccountState primary_account_state_;
  base::Optional<AccountInfo> account_info_from_gaia_id_;
  AccountState account_state_from_gaia_id_;
  base::Optional<std::string> access_token_;
  GoogleServiceAuthError access_token_error_;

  AccountTrackerService* account_tracker() { return &account_tracker_; }
  SigninManagerBase* signin_manager() { return &signin_manager_; }
  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  FakeAccountFetcherService account_fetcher_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;
#if defined(OS_CHROMEOS)
  SigninManagerBase signin_manager_;
#else
  SigninManager signin_manager_;
#endif
  GaiaCookieManagerService gaia_cookie_manager_service_;

  identity::IdentityTestEnvironment identity_test_environment_;
  service_manager::TestConnectorFactory test_connector_factory_;
  IdentityService service_;

  DISALLOW_COPY_AND_ASSIGN(IdentityAccessorImplTest);
};

// Check that the primary account info is null if not signed in.
TEST_F(IdentityAccessorImplTest, GetPrimaryAccountInfoNotSignedIn) {
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityAccessorImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(primary_account_info_);
}

// Check that the primary account info has expected values if signed in without
// a refresh token available.
TEST_F(IdentityAccessorImplTest, GetPrimaryAccountInfoSignedInNoRefreshToken) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityAccessorImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_info_->account_id);
  EXPECT_EQ(kTestGaiaId, primary_account_info_->gaia);
  EXPECT_EQ(kTestEmail, primary_account_info_->email);
  EXPECT_FALSE(primary_account_state_.has_refresh_token);
  EXPECT_TRUE(primary_account_state_.is_primary_account);
}

// Check that the primary account info has expected values if signed in with a
// refresh token available.
TEST_F(IdentityAccessorImplTest, GetPrimaryAccountInfoSignedInRefreshToken) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityAccessorImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_info_->account_id);
  EXPECT_EQ(kTestGaiaId, primary_account_info_->gaia);
  EXPECT_EQ(kTestEmail, primary_account_info_->email);
  EXPECT_TRUE(primary_account_state_.has_refresh_token);
  EXPECT_TRUE(primary_account_state_.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() returns immediately in the
// case where the primary account is available when the call is received.
TEST_F(IdentityAccessorImplTest, GetPrimaryAccountWhenAvailableSignedIn) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);

  AccountInfo account_info;
  AccountState account_state;
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityAccessorImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));
  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() returns the expected account
// info in the case where the primary account is made available *after* the
// call is received.
TEST_F(IdentityAccessorImplTest, GetPrimaryAccountWhenAvailableSignInLater) {
  AccountInfo account_info;
  AccountState account_state;

  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityAccessorImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));

  // Verify that the primary account info is not currently available (this also
  // serves to ensure that the preceding call has been received by the
  // IdentityAccessor before proceeding).
  base::RunLoop run_loop2;
  GetIdentityAccessorImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityAccessorImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_FALSE(primary_account_info_);

  // Make the primary account available and check that the callback is invoked
  // as expected.
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() returns the expected account
// info in the case where signin is done before the call is received but the
// refresh token is made available only *after* the call is received.
TEST_F(IdentityAccessorImplTest,
       GetPrimaryAccountWhenAvailableTokenAvailableLater) {
  AccountInfo account_info;
  AccountState account_state;

  // Sign in, but don't set the refresh token yet.
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityAccessorImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));

  // Verify that the primary account info is present, but that the primary
  // account is not yet considered available (this also
  // serves to ensure that the preceding call has been received by the
  // IdentityAccessor before proceeding).
  base::RunLoop run_loop2;
  GetIdentityAccessorImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityAccessorImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();

  EXPECT_TRUE(primary_account_info_);
  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            primary_account_info_->account_id);
  EXPECT_TRUE(account_info.account_id.empty());

  // Set the refresh token and check that the callback is invoked as expected
  // (i.e., the primary account is now considered available).
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() returns the expected account info
// in the case where the token is available before the call is received but the
// account is made authenticated only *after* the call is received. This test is
// relevant only on non-ChromeOS platforms, as the flow being tested here is not
// possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(IdentityAccessorImplTest,
       GetPrimaryAccountWhenAvailableAuthenticationAvailableLater) {
  AccountInfo account_info;
  AccountState account_state;

  // Set the refresh token, but don't sign in yet.
  std::string account_id_to_use =
      account_tracker()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id_to_use, kTestRefreshToken);
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityAccessorImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));

  // Verify that the account is present and has a refresh token, but that the
  // primary account is not yet considered available (this also serves to ensure
  // that the preceding call has been received by the IdentityAccessor before
  // proceeding).
  base::RunLoop run_loop2;
  GetIdentityAccessorImpl()->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::BindRepeating(
          &IdentityAccessorImplTest::OnReceivedAccountInfoFromGaiaId,
          base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();

  EXPECT_TRUE(account_info_from_gaia_id_);
  EXPECT_EQ(account_id_to_use, account_info_from_gaia_id_->account_id);
  EXPECT_EQ(kTestGaiaId, account_info_from_gaia_id_->gaia);
  EXPECT_EQ(kTestEmail, account_info_from_gaia_id_->email);
  EXPECT_TRUE(account_state_from_gaia_id_.has_refresh_token);
  EXPECT_FALSE(account_state_from_gaia_id_.is_primary_account);

  EXPECT_TRUE(account_info.account_id.empty());

  // Sign the user in and check that the callback is invoked as expected (i.e.,
  // the primary account is now considered available). Note that it is necessary
  // to call SignIn() here to ensure that GoogleSigninSucceeded() is fired by
  // the fake signin manager.
  SigninManager::FromSigninManagerBase(signin_manager())
      ->OnExternalSigninCompleted(kTestEmail);

  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}
#endif

// Check that GetPrimaryAccountWhenAvailable() returns the expected account
// info to all callers in the case where the primary account is made available
// after multiple overlapping calls have been received.
TEST_F(IdentityAccessorImplTest,
       GetPrimaryAccountWhenAvailableOverlappingCalls) {
  AccountInfo account_info1;
  AccountState account_state1;
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityAccessorImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info1), base::Unretained(&account_state1)));

  AccountInfo account_info2;
  AccountState account_state2;
  base::RunLoop run_loop2;
  GetIdentityAccessorImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityAccessorImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop2.QuitClosure(),
      base::Unretained(&account_info2), base::Unretained(&account_state2)));

  // Verify that the primary account info is not currently available (this also
  // serves to ensure that the preceding call has been received by the
  // IdentityAccessor before proceeding).
  base::RunLoop run_loop3;
  GetIdentityAccessorImpl()->GetPrimaryAccountInfo(base::BindRepeating(
      &IdentityAccessorImplTest::OnReceivedPrimaryAccountInfo,
      base::Unretained(this), run_loop3.QuitClosure()));
  run_loop3.Run();
  EXPECT_FALSE(primary_account_info_);

  // Make the primary account available and check that the callbacks are invoked
  // as expected.
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  run_loop.Run();
  run_loop2.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info1.account_id);
  EXPECT_EQ(kTestGaiaId, account_info1.gaia);
  EXPECT_EQ(kTestEmail, account_info1.email);
  EXPECT_TRUE(account_state1.has_refresh_token);
  EXPECT_TRUE(account_state1.is_primary_account);

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info2.account_id);
  EXPECT_EQ(kTestGaiaId, account_info2.gaia);
  EXPECT_EQ(kTestEmail, account_info2.email);
  EXPECT_TRUE(account_state2.has_refresh_token);
  EXPECT_TRUE(account_state2.is_primary_account);
}

// Check that GetPrimaryAccountWhenAvailable() doesn't return the account as
// available if the refresh token has an auth error.
TEST_F(IdentityAccessorImplTest,
       GetPrimaryAccountWhenAvailableRefreshTokenHasAuthError) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  token_service()->UpdateAuthErrorForTesting(
      signin_manager()->GetAuthenticatedAccountId(),
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  AccountInfo account_info;
  AccountState account_state;
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetPrimaryAccountWhenAvailable(base::BindRepeating(
      &IdentityAccessorImplTest::OnPrimaryAccountAvailable,
      base::Unretained(this), run_loop.QuitClosure(),
      base::Unretained(&account_info), base::Unretained(&account_state)));

  // Flush the IdentityAccessor and check that the callback didn't fire.
  FlushIdentityAccessorImplForTesting();
  EXPECT_TRUE(account_info.account_id.empty());

  // Clear the auth error, update credentials, and check that the callback
  // fires.
  token_service()->UpdateAuthErrorForTesting(
      signin_manager()->GetAuthenticatedAccountId(), GoogleServiceAuthError());
  token_service()->UpdateCredentials(
      signin_manager()->GetAuthenticatedAccountId(), kTestRefreshToken);
  run_loop.Run();

  EXPECT_EQ(signin_manager()->GetAuthenticatedAccountId(),
            account_info.account_id);
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
  EXPECT_TRUE(account_state.has_refresh_token);
  EXPECT_TRUE(account_state.is_primary_account);
}

// Check that the account info for a given GAIA ID is null if that GAIA ID is
// unknown.
TEST_F(IdentityAccessorImplTest, GetAccountInfoForUnknownGaiaID) {
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::BindRepeating(
          &IdentityAccessorImplTest::OnReceivedAccountInfoFromGaiaId,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(account_info_from_gaia_id_);
}

// Check that the account info for a given GAIA ID has expected values if that
// GAIA ID is known and there is no refresh token available for it.
TEST_F(IdentityAccessorImplTest, GetAccountInfoForKnownGaiaIdNoRefreshToken) {
  std::string account_id =
      account_tracker()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::BindRepeating(
          &IdentityAccessorImplTest::OnReceivedAccountInfoFromGaiaId,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(account_info_from_gaia_id_);
  EXPECT_EQ(account_id, account_info_from_gaia_id_->account_id);
  EXPECT_EQ(kTestGaiaId, account_info_from_gaia_id_->gaia);
  EXPECT_EQ(kTestEmail, account_info_from_gaia_id_->email);
  EXPECT_FALSE(account_state_from_gaia_id_.has_refresh_token);
  EXPECT_FALSE(account_state_from_gaia_id_.is_primary_account);
}

// Check that the account info for a given GAIA ID has expected values if that
// GAIA ID is known and has a refresh token available.
TEST_F(IdentityAccessorImplTest, GetAccountInfoForKnownGaiaIdRefreshToken) {
  std::string account_id =
      account_tracker()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, kTestRefreshToken);
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetAccountInfoFromGaiaId(
      kTestGaiaId,
      base::BindRepeating(
          &IdentityAccessorImplTest::OnReceivedAccountInfoFromGaiaId,
          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(account_info_from_gaia_id_);
  EXPECT_EQ(account_id, account_info_from_gaia_id_->account_id);
  EXPECT_EQ(kTestGaiaId, account_info_from_gaia_id_->gaia);
  EXPECT_EQ(kTestEmail, account_info_from_gaia_id_->email);
  EXPECT_TRUE(account_state_from_gaia_id_.has_refresh_token);
  EXPECT_FALSE(account_state_from_gaia_id_.is_primary_account);
}

// Check that the expected error is received if requesting an access token when
// not signed in.
TEST_F(IdentityAccessorImplTest, GetAccessTokenNotSignedIn) {
  base::RunLoop run_loop;
  GetIdentityAccessorImpl()->GetAccessToken(
      kTestGaiaId, ScopeSet(), "dummy_consumer",
      base::BindRepeating(&IdentityAccessorImplTest::OnReceivedAccessToken,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(access_token_);
  EXPECT_EQ(GoogleServiceAuthError::State::USER_NOT_SIGNED_UP,
            access_token_error_.state());
}

// Check that the expected access token is received if requesting an access
// token when signed in.
TEST_F(IdentityAccessorImplTest, GetAccessTokenSignedIn) {
  signin_manager()->SetAuthenticatedAccountInfo(kTestGaiaId, kTestEmail);
  std::string account_id = signin_manager()->GetAuthenticatedAccountId();
  token_service()->UpdateCredentials(account_id, kTestRefreshToken);
  token_service()->set_auto_post_fetch_response_on_message_loop(true);
  base::RunLoop run_loop;

  GetIdentityAccessorImpl()->GetAccessToken(
      account_id, ScopeSet(), "dummy_consumer",
      base::BindRepeating(&IdentityAccessorImplTest::OnReceivedAccessToken,
                          base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(access_token_);
  EXPECT_EQ(kTestAccessToken, access_token_.value());
  EXPECT_EQ(GoogleServiceAuthError::State::NONE, access_token_error_.state());
}

}  // namespace
}  // namespace identity
