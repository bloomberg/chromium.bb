// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/accounts_mutator_impl.h"

#include "base/message_loop/message_loop.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "services/identity/public/cpp/accounts_mutator_impl.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestGaiaId[] = "gaia-id-test_user@test.com";
const char kTestGaiaId2[] = "gaia-id-test_user-2@test.com";
const char kTestEmail[] = "test_user@test.com";
const char kTestEmail2[] = "test_user@test-2.com";
const char kRefreshToken[] = "refresh_token";
const char kRefreshToken2[] = "refresh_token_2";

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

  void set_on_refresh_token_updated_callback(
      base::OnceCallback<void(const std::string&)> callback) {
    on_refresh_token_updated_callback_ = std::move(callback);
  }

  void set_on_refresh_token_removed_callback(
      base::OnceCallback<void(const std::string&)> callback) {
    on_refresh_token_removed_callback_ = std::move(callback);
  }

 private:
  // identity::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(
      const AccountInfo& account_info) override {
    if (on_refresh_token_updated_callback_)
      std::move(on_refresh_token_updated_callback_)
          .Run(account_info.account_id);
  }

  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override {
    if (on_refresh_token_removed_callback_)
      std::move(on_refresh_token_removed_callback_).Run(account_id);
  }

  identity::IdentityManager* identity_manager_;
  base::OnceCallback<void(const std::string&)>
      on_refresh_token_updated_callback_;
  base::OnceCallback<void(const std::string&)>
      on_refresh_token_removed_callback_;
};

}  // namespace

namespace identity {
class AccountsMutatorTest : public testing::Test {
 public:
  AccountsMutatorTest() : identity_manager_observer_(identity_manager()) {}

  ~AccountsMutatorTest() override {}

  identity::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  TestIdentityManagerObserver* identity_manager_observer() {
    return &identity_manager_observer_;
  }

  AccountsMutator* accounts_mutator() {
    return identity_manager()->GetAccountsMutator();
  }

 private:
  base::MessageLoop message_loop_;
  identity::IdentityTestEnvironment identity_test_env_;
  TestIdentityManagerObserver identity_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(AccountsMutatorTest);
};

TEST_F(AccountsMutatorTest, Basic) {
  // Should not crash.
}

// Test that a new account gets added to the AccountTrackerService when calling
// AddOrUpdateAccount() and that a new refresh token becomes available for the
// passed account_id when adding an account for the first time.
TEST_F(AccountsMutatorTest, AddOrUpdateAccount_AddNewAccount) {
  // Abort the test if the current platform does not support accounts mutation.
  if (!accounts_mutator())
    return;

  base::RunLoop run_loop;
  identity_manager_observer()->set_on_refresh_token_updated_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure, const std::string& account_id) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  std::string account_id = accounts_mutator()->AddOrUpdateAccount(
      kTestGaiaId, kTestEmail, kRefreshToken,
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop.Run();

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id));

  AccountInfo account_info =
      identity_manager()
          ->FindAccountInfoForAccountWithRefreshTokenByAccountId(account_id)
          .value();
  EXPECT_EQ(account_info.account_id, account_id);
  EXPECT_EQ(account_info.email, kTestEmail);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 1U);
}

// Test that no account gets added to the AccountTrackerService  when calling
// AddOrUpdateAccount() if there's an account already tracked for a given id,
// and that its refresh token gets updated if a different one is passed.
TEST_F(AccountsMutatorTest, AddOrUpdateAccount_UpdateExistingAccount) {
  // Abort the test if the current platform does not support accounts mutation.
  if (!accounts_mutator())
    return;

  // First of all add the account to the account tracker service.
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_refresh_token_updated_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure, const std::string& account_id) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  std::string account_id = accounts_mutator()->AddOrUpdateAccount(
      kTestGaiaId, kTestEmail, kRefreshToken,
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop.Run();

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id));
  AccountInfo account_info =
      identity_manager()
          ->FindAccountInfoForAccountWithRefreshTokenByAccountId(account_id)
          .value();
  EXPECT_EQ(account_info.account_id, account_id);
  EXPECT_EQ(account_info.email, kTestEmail);
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 1U);

  // Now try adding the account again with the same account id but with
  // different information, and check that the account gets updated.
  base::RunLoop run_loop2;
  identity_manager_observer()->set_on_refresh_token_updated_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const std::string& expected_account_id,
             const std::string& added_account_id) {
            EXPECT_EQ(added_account_id, expected_account_id);
            std::move(quit_closure).Run();
          },
          run_loop2.QuitClosure(), account_id));

  // The internals of IdentityService is migrating from email to gaia id
  // as the account id. Detect whether the current plaform has completed
  // the migration.
  const bool use_gaia_as_account_id = account_id == account_info.gaia;

  // If the system uses gaia id as account_id, then change the email and
  // the |is_under_advanced_protection| field. Otherwise only change the
  // latter. In all case, no new account should be created.
  const char* const maybe_updated_email =
      use_gaia_as_account_id ? kTestEmail2 : kTestEmail;

  accounts_mutator()->AddOrUpdateAccount(
      kTestGaiaId, maybe_updated_email, kRefreshToken,
      /*is_under_advanced_protection=*/true,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop2.Run();

  // No new accounts should be created, just the information should be updated.
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 1U);
  AccountInfo updated_account_info =
      identity_manager()
          ->FindAccountInfoForAccountWithRefreshTokenByAccountId(account_id)
          .value();
  EXPECT_EQ(account_info.account_id, updated_account_info.account_id);
  EXPECT_EQ(account_info.gaia, updated_account_info.gaia);
  EXPECT_EQ(updated_account_info.email, maybe_updated_email);
  if (use_gaia_as_account_id) {
    EXPECT_NE(updated_account_info.email, account_info.email);
    EXPECT_EQ(updated_account_info.email, kTestEmail2);
  }
  EXPECT_NE(account_info.is_under_advanced_protection,
            updated_account_info.is_under_advanced_protection);
}

// Test that attempting to remove a non-existing account should not result in
// firing any callback from AccountTrackerService or ProfileOAuth2TokenService.
TEST_F(AccountsMutatorTest, RemoveAccount_NonExistingAccount) {
  // Abort the test if the current platform does not support accounts mutation.
  if (!accounts_mutator())
    return;

  base::RunLoop run_loop;
  identity_manager_observer()->set_on_refresh_token_updated_callback(
      base::BindOnce([](const std::string& account_id) {
        // This callback should not be invoked now.
        EXPECT_TRUE(false);
      }));

  accounts_mutator()->RemoveAccount(
      kTestGaiaId, signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop.RunUntilIdle();

  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(kTestGaiaId));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          kTestGaiaId));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0U);
}

// Test that attempting to remove an existing account should result in firing
// the right callbacks from AccountTrackerService or ProfileOAuth2TokenService.
TEST_F(AccountsMutatorTest, RemoveAccount_ExistingAccount) {
  // Abort the test if the current platform does not support accounts mutation.
  if (!accounts_mutator())
    return;

  // First of all add the account to the account tracker service.
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_refresh_token_updated_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure, const std::string& account_id) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  std::string account_id = accounts_mutator()->AddOrUpdateAccount(
      kTestGaiaId, kTestEmail, kRefreshToken,
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop.Run();

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 1U);

  // Now remove the account that we just added.
  base::RunLoop run_loop2;
  identity_manager_observer()->set_on_refresh_token_removed_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             const std::string& expected_account_id,
             const std::string& removed_account_id) {
            EXPECT_EQ(removed_account_id, expected_account_id);
            std::move(quit_closure).Run();
          },
          run_loop2.QuitClosure(), account_id));

  accounts_mutator()->RemoveAccount(
      account_id, signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop2.Run();

  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0U);
}

// Test that attempting to remove all accounts removes all the tokens from the
// PO2TS and every account from the AccountTrackerService.
TEST_F(AccountsMutatorTest, RemoveAllAccounts) {
  // Abort the test if the current platform does not support accounts mutation.
  if (!accounts_mutator())
    return;

  // First of all the first account to the account tracker service.
  base::RunLoop run_loop;
  identity_manager_observer()->set_on_refresh_token_updated_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure, const std::string& account_id) {
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure()));

  std::string account_id = accounts_mutator()->AddOrUpdateAccount(
      kTestGaiaId, kTestEmail, kRefreshToken,
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop.Run();

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 1U);

  // Now add the second account.
  base::RunLoop run_loop2;
  identity_manager_observer()->set_on_refresh_token_updated_callback(
      base::BindOnce(
          [](base::OnceClosure quit_closure, const std::string& account_id) {
            std::move(quit_closure).Run();
          },
          run_loop2.QuitClosure()));

  std::string account_id2 = accounts_mutator()->AddOrUpdateAccount(
      kTestGaiaId2, kTestEmail2, kRefreshToken2,
      /*is_under_advanced_protection=*/false,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop2.Run();

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 2U);

  // Now remove everything and check that there are no lingering accounts, nor
  // refresh tokens associated to |kTestGaiaId| and |kTestGaiaId2| afterwards.
  base::RunLoop run_loop3;
  accounts_mutator()->RemoveAllAccounts(
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  run_loop3.RunUntilIdle();

  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(kTestGaiaId));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(kTestGaiaId2));
  EXPECT_EQ(identity_manager()->GetAccountsWithRefreshTokens().size(), 0U);
}

}  // namespace identity
