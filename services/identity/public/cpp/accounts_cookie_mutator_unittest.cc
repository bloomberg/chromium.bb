// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_cookie_mutator.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_task_environment.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestUnavailableAccountId[] = "unavailable_account_id";
const char kTestAccountEmail[] = "test_user@test.com";
const char kTestAccessToken[] = "access_token";
const char kTestUberToken[] = "test_uber_token";

enum class AccountsCookiesMutatorAction {
  kAddAccountToCookie,
};

// Class that observes updates from identity::IdentityManager.
class TestIdentityManagerObserver : public identity::IdentityManager::Observer {
 public:
  explicit TestIdentityManagerObserver(
      identity::IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    identity_manager_->AddObserver(this);
  }
  ~TestIdentityManagerObserver() override {
    identity_manager_->RemoveObserver(this);
  }

  void set_on_add_account_to_cookie_completed_callback(
      base::OnceCallback<void(const std::string&,
                              const GoogleServiceAuthError&)> callback) {
    on_add_account_to_cookie_completed_callback_ = std::move(callback);
  }

 private:
  // identity::IdentityManager::Observer:
  void OnAddAccountToCookieCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) override {
    if (on_add_account_to_cookie_completed_callback_)
      std::move(on_add_account_to_cookie_completed_callback_)
          .Run(account_id, error);
  }

  identity::IdentityManager* identity_manager_;
  base::OnceCallback<void(const std::string&, const GoogleServiceAuthError&)>
      on_add_account_to_cookie_completed_callback_;
};

}  // namespace

namespace identity {
class AccountsCookieMutatorTest : public testing::Test {
 public:
  AccountsCookieMutatorTest()
      : identity_test_env_(&test_url_loader_factory_),
        identity_manager_observer_(identity_test_env_.identity_manager()) {}

  ~AccountsCookieMutatorTest() override {}

  // Make an account available and returns the account ID.
  std::string AddAcountWithRefreshToken(const std::string& email) {
    return identity_test_env_.MakeAccountAvailable(email).account_id;
  }

  // Feed the TestURLLoaderFactory with the responses for the requests that will
  // be created by UberTokenFetcher when mergin accounts into the cookie jar.
  void PrepareURLLoaderResponsesForAction(AccountsCookiesMutatorAction action) {
    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()
            ->oauth1_login_url()
            .Resolve(base::StringPrintf("?source=%s&issueuberauth=1",
                                        GaiaConstants::kChromeSource))
            .spec(),
        kTestUberToken, net::HTTP_OK);

    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()
            ->GetCheckConnectionInfoURLWithSource(GaiaConstants::kChromeSource)
            .spec(),
        std::string(), net::HTTP_OK);

    test_url_loader_factory_.AddResponse(
        GaiaUrls::GetInstance()
            ->merge_session_url()
            .Resolve(base::StringPrintf(
                "?uberauth=%s&continue=http://www.google.com&source=%s",
                kTestUberToken, GaiaConstants::kChromeSource))
            .spec(),
        std::string(), net::HTTP_OK);
  }

  identity::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  TestIdentityManagerObserver* identity_manager_observer() {
    return &identity_manager_observer_;
  }

  AccountsCookieMutator* accounts_cookie_mutator() {
    return identity_test_env_.identity_manager()->GetAccountsCookieMutator();
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  identity::IdentityTestEnvironment identity_test_env_;
  TestIdentityManagerObserver identity_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(AccountsCookieMutatorTest);
};

// Test that adding a non existing account without providing an access token
// results in an error due to such account not being available.
TEST_F(AccountsCookieMutatorTest, AddAccountToCookie_NonExistingAccount) {
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_add_account_to_cookie_completed_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure, const std::string& account_id,
             const GoogleServiceAuthError& error) {
            EXPECT_EQ(account_id, kTestUnavailableAccountId);
            // The account was not previously available and no access token was
            // provided when adding it to the cookie jar: expect an error.
            EXPECT_EQ(error.state(),
                      GoogleServiceAuthError::USER_NOT_SIGNED_UP);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  accounts_cookie_mutator()->AddAccountToCookie(kTestUnavailableAccountId,
                                                gaia::GaiaSource::kChrome);
  run_loop.Run();
}

// Test that adding an already available account without providing an access
// token results in such account being successfully merged into the cookie jar.
TEST_F(AccountsCookieMutatorTest, AddAccountToCookie_ExistingAccount) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kAddAccountToCookie);

  std::string account_id = AddAcountWithRefreshToken(kTestAccountEmail);
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_add_account_to_cookie_completed_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const std::string& expected_account_id,
             const std::string& account_id,
             const GoogleServiceAuthError& error) {
            EXPECT_EQ(account_id, expected_account_id);
            // The account was previously available: expect success.
            EXPECT_EQ(error.state(), GoogleServiceAuthError::NONE);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), account_id));

  accounts_cookie_mutator()->AddAccountToCookie(account_id,
                                                gaia::GaiaSource::kChrome);
  identity_test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      account_id, kTestAccessToken,
      base::Time::Now() + base::TimeDelta::FromHours(1));

  run_loop.Run();
}

// Test that adding a non existing account along with an access token, results
// on such account being successfully merged into the cookie jar.
TEST_F(AccountsCookieMutatorTest,
       AddAccountToCookieWithAccessToken_NonExistingAccount) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kAddAccountToCookie);

  base::RunLoop run_loop;
  identity_manager_observer()->set_on_add_account_to_cookie_completed_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure, const std::string& account_id,
             const GoogleServiceAuthError& error) {
            EXPECT_EQ(account_id, kTestUnavailableAccountId);
            // Trying to add an non-available account with a valid token to the
            // cookie jar should result in the account being merged.
            EXPECT_EQ(error.state(), GoogleServiceAuthError::NONE);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  accounts_cookie_mutator()->AddAccountToCookieWithToken(
      kTestUnavailableAccountId, kTestAccessToken, gaia::GaiaSource::kChrome);
  run_loop.Run();
}

// Test that adding an already available account along with an access token,
// results in such account being successfully merged into the cookie jar.
TEST_F(AccountsCookieMutatorTest,
       AddAccountToCookieWithAccessToken_ExistingAccount) {
  PrepareURLLoaderResponsesForAction(
      AccountsCookiesMutatorAction::kAddAccountToCookie);

  std::string account_id = AddAcountWithRefreshToken(kTestAccountEmail);
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_add_account_to_cookie_completed_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const std::string& expected_account_id,
             const std::string& account_id,
             const GoogleServiceAuthError& error) {
            EXPECT_EQ(account_id, expected_account_id);
            // Trying to add a previously available account with a valid token
            // to the cookie jar should also result in the account being merged.
            EXPECT_EQ(error.state(), GoogleServiceAuthError::NONE);
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), account_id));

  accounts_cookie_mutator()->AddAccountToCookieWithToken(
      account_id, kTestAccessToken, gaia::GaiaSource::kChrome);

  run_loop.Run();
}

}  // namespace identity
