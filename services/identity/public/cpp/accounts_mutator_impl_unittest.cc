// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_mutator_impl.h"

#include "base/message_loop/message_loop.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/identity/public/cpp/accounts_mutator_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestGaiaId[] = "gaia-id-test_user@test.com";
const char kTestGaiaId2[] = "gaia-id-test_user-2@test.com";
const char kTestEmail[] = "test_user@test.com";
const char kTestEmail2[] = "test_user@test-2.com";
const char kRefreshToken[] = "refresh_token";
const char kRefreshToken2[] = "refresh_token_2";

// Class that observes updates from ProfileOAuth2TokenService.
class TestTokenServiceObserver : public OAuth2TokenService::Observer {
 public:
  explicit TestTokenServiceObserver(OAuth2TokenService* token_service)
      : token_service_(token_service) {
    token_service_->AddObserver(this);
  }
  ~TestTokenServiceObserver() override { token_service_->RemoveObserver(this); }

  void set_on_refresh_tokens_available_callback(
      base::RepeatingCallback<void(const std::string&)> callback) {
    on_refresh_tokens_available_callback_ = std::move(callback);
  }

  void set_on_refresh_tokens_revoked_callback(
      base::RepeatingCallback<void(const std::string&)> callback) {
    on_refresh_tokens_revoked_callback_ = std::move(callback);
  }

  void set_on_refresh_tokens_loaded_callback(base::OnceClosure callback) {
    on_refresh_tokens_loaded_callback_ = std::move(callback);
  }

 private:
  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override {
    if (on_refresh_tokens_available_callback_)
      std::move(on_refresh_tokens_available_callback_).Run(account_id);
  }

  void OnRefreshTokenRevoked(const std::string& account_id) override {
    if (on_refresh_tokens_revoked_callback_)
      std::move(on_refresh_tokens_revoked_callback_).Run(account_id);
  }

  void OnRefreshTokensLoaded() override {
    if (on_refresh_tokens_loaded_callback_)
      std::move(on_refresh_tokens_loaded_callback_).Run();
  }

  OAuth2TokenService* token_service_;
  base::RepeatingCallback<void(const std::string&)>
      on_refresh_tokens_available_callback_;
  base::RepeatingCallback<void(const std::string&)>
      on_refresh_tokens_revoked_callback_;
  base::OnceClosure on_refresh_tokens_loaded_callback_;
};

}  // namespace

namespace identity {
class AccountsMutatorImplTest : public testing::Test {
 public:
  AccountsMutatorImplTest()
      : signin_client_(&pref_service_),
        token_service_(&pref_service_),
        token_service_observer_(&token_service_),
        accounts_mutator_(&token_service_) {
    ProfileOAuth2TokenService::RegisterProfilePrefs(pref_service_.registry());

    AccountTrackerService::RegisterPrefs(pref_service_.registry());
    account_tracker_service_.Initialize(&pref_service_, base::FilePath());

    AccountFetcherService::RegisterPrefs(pref_service_.registry());
    account_fetcher_ = std::make_unique<FakeAccountFetcherService>();
    account_fetcher_->Initialize(&signin_client_, &token_service_,
                                 &account_tracker_service_,
                                 std::make_unique<TestImageDecoder>());

    // Need to enable network fetches so that AccountFetcherService will talk
    // to the AccountTrackerService to remove the account when revoking tokens.
    account_fetcher_->EnableNetworkFetchesForTest();
  }

  ~AccountsMutatorImplTest() override {
    account_fetcher_->Shutdown();
    account_fetcher_.reset();
  }

  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

  AccountTrackerService* account_tracker_service() {
    return &account_tracker_service_;
  }

  TestTokenServiceObserver* token_service_observer() {
    return &token_service_observer_;
  }

  AccountsMutatorImpl* accounts_mutator() { return &accounts_mutator_; }

 private:
  base::MessageLoop message_loop_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  AccountTrackerService account_tracker_service_;
  std::unique_ptr<FakeAccountFetcherService> account_fetcher_;
  TestTokenServiceObserver token_service_observer_;
  AccountsMutatorImpl accounts_mutator_;

  DISALLOW_COPY_AND_ASSIGN(AccountsMutatorImplTest);
};

TEST_F(AccountsMutatorImplTest, Basic) {
  // Should not crash.
}

// Test that attempting to remove an existing account should result in firing
// the right callbacks from AccountTrackerService or ProfileOAuth2TokenService.
TEST_F(AccountsMutatorImplTest, RemoveAccount_ExistingAccount) {
  // First of all add the account to the account tracker service.
  base::RunLoop run_loop;
  token_service_observer()->set_on_refresh_tokens_available_callback(
      base::BindRepeating([](base::RunLoop* loop,
                             const std::string& account_id) { loop->Quit(); },
                          base::Unretained(&run_loop)));

  // TODO(crbug.com/907901): Migrate this to
  // AccountsMutator::AddOrUpdateAccount() once available.
  std::string account_id =
      account_tracker_service()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, kRefreshToken);
  run_loop.Run();

  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id));
  EXPECT_FALSE(token_service()->RefreshTokenHasError(account_id));
  EXPECT_EQ(account_tracker_service()->GetAccounts().size(), 1U);

  // Now remove the account that we just added.
  base::RunLoop run_loop2;
  token_service_observer()->set_on_refresh_tokens_revoked_callback(
      base::BindRepeating(
          [](base::RunLoop* loop, const std::string& expected_id,
             const std::string& removed_account_id) {
            EXPECT_EQ(removed_account_id, expected_id);
            loop->Quit();
          },
          base::Unretained(&run_loop2), account_id));

  accounts_mutator()->RemoveAccount(account_id);
  run_loop2.Run();

  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id));
  EXPECT_FALSE(token_service()->RefreshTokenHasError(account_id));
  EXPECT_EQ(account_tracker_service()->GetAccounts().size(), 0U);
}

// Test that attempting to remove all accounts removes all the tokens from the
// PO2TS and every account from the AccountTrackerService.
TEST_F(AccountsMutatorImplTest, RemoveAllAccounts) {
  // First of all the first account to the account tracker service.
  base::RunLoop run_loop;
  token_service_observer()->set_on_refresh_tokens_available_callback(
      base::BindRepeating([](base::RunLoop* loop,
                             const std::string& account_id) { loop->Quit(); },
                          base::Unretained(&run_loop)));

  // TODO(crbug.com/907901): Migrate this to
  // AccountsMutator::AddOrUpdateAccount() once available.
  std::string account_id =
      account_tracker_service()->SeedAccountInfo(kTestGaiaId, kTestEmail);
  token_service()->UpdateCredentials(account_id, kRefreshToken);
  run_loop.Run();

  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id));
  EXPECT_FALSE(token_service()->RefreshTokenHasError(account_id));
  EXPECT_EQ(account_tracker_service()->GetAccounts().size(), 1U);

  // Now add the second account.
  base::RunLoop run_loop2;
  token_service_observer()->set_on_refresh_tokens_available_callback(
      base::BindRepeating([](base::RunLoop* loop,
                             const std::string& account_id) { loop->Quit(); },
                          base::Unretained(&run_loop2)));

  // TODO(crbug.com/907901): Migrate this to
  // AccountsMutator::AddOrUpdateAccount() once available.
  std::string account_id2 =
      account_tracker_service()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  token_service()->UpdateCredentials(account_id2, kRefreshToken2);
  run_loop2.Run();

  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id2));
  EXPECT_FALSE(token_service()->RefreshTokenHasError(account_id2));
  EXPECT_EQ(account_tracker_service()->GetAccounts().size(), 2U);

  // Now remove everything and check that there are no lingering accounts, nor
  // refresh tokens associated to |kTestGaiaId| and |kTestGaiaId2| afterwards.
  base::RunLoop run_loop3;
  accounts_mutator()->RemoveAllAccounts();
  run_loop3.RunUntilIdle();

  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(kTestGaiaId));
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(kTestGaiaId2));
  EXPECT_EQ(account_tracker_service()->GetAccounts().size(), 0U);
}

}  // namespace identity
