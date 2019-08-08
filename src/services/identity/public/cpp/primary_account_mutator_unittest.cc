// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_mutator.h"

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/scoped_task_environment.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "testing/platform_test.h"

namespace {

// Constants used by the different tests.
const char kUnknownAccountId[] = "{unknown account id}";
const char kPrimaryAccountEmail[] = "primary.account@example.com";
const char kAnotherAccountEmail[] = "another.account@example.com";

// All account consistency methods that are tested by those unit tests when
// testing ClearPrimaryAccount method.
const signin::AccountConsistencyMethod kTestedAccountConsistencyMethods[] = {
    signin::AccountConsistencyMethod::kDisabled,
    signin::AccountConsistencyMethod::kMirror,
    signin::AccountConsistencyMethod::kDiceMigration,
    signin::AccountConsistencyMethod::kDice,
};

// See RunClearPrimaryAccountTest().
enum class AuthExpectation { kAuthNormal, kAuthError };
enum class RemoveAccountExpectation { kKeepAll, kRemovePrimary, kRemoveAll };

// This callback will be invoked every time the IdentityManager::Observer
// method OnPrimaryAccountCleared is invoked. The parameter will be a
// reference to the still valid primary account that was cleared.
using PrimaryAccountClearedCallback =
    base::RepeatingCallback<void(const CoreAccountInfo&)>;

// This callback will be invoked every time the IdentityManager::Observer
// method OnRefreshTokenRemoved is invoked. The parameter will be a reference
// to the account_id whose token was removed.
using RefreshTokenRemovedCallback =
    base::RepeatingCallback<void(const std::string&)>;

// Helper IdentityManager::Observer that forwards some events to the
// callback passed to the constructor.
class ClearPrimaryAccountTestObserver
    : public identity::IdentityManager::Observer {
 public:
  ClearPrimaryAccountTestObserver(
      identity::IdentityManager* identity_manager,
      PrimaryAccountClearedCallback on_primary_account_cleared,
      RefreshTokenRemovedCallback on_refresh_token_removed)
      : on_primary_account_cleared_(std::move(on_primary_account_cleared)),
        on_refresh_token_removed_(std::move(on_refresh_token_removed)),
        scoped_observer_(this) {
    DCHECK(on_primary_account_cleared_);
    DCHECK(on_refresh_token_removed_);
    scoped_observer_.Add(identity_manager);
  }

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountCleared(const CoreAccountInfo& account_info) override {
    on_primary_account_cleared_.Run(account_info);
  }

  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override {
    on_refresh_token_removed_.Run(account_id);
  }

 private:
  PrimaryAccountClearedCallback on_primary_account_cleared_;
  RefreshTokenRemovedCallback on_refresh_token_removed_;
  ScopedObserver<identity::IdentityManager, identity::IdentityManager::Observer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(ClearPrimaryAccountTestObserver);
};

// Helper for testing of ClearPrimaryAccount(). This function requires lots
// of tests due to having different behaviors based on its arguments. But the
// setup and execution of these test is all the boiler plate you see here:
// 1) Ensure you have 2 accounts, both with refresh tokens
// 2) Clear the primary account
// 3) Assert clearing succeeds and refresh tokens are optionally removed based
//    on arguments.
//
// Optionally, it's possible to specify whether a normal auth process will
// take place, or whether an auth error should happen, useful for some tests.
void RunClearPrimaryAccountTest(
    signin::AccountConsistencyMethod account_consistency_method,
    identity::PrimaryAccountMutator::ClearAccountsAction account_action,
    RemoveAccountExpectation account_expectation,
    AuthExpectation auth_expection = AuthExpectation::kAuthNormal) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment(
      /*test_url_loader_factory=*/nullptr, /*pref_service=*/nullptr,
      account_consistency_method);

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  // With the exception of ClearPrimaryAccount_AuthInProgress, every other
  // ClearPrimaryAccount_* test requires a primary account to be signed in.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  AccountInfo account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);
  EXPECT_TRUE(
      primary_account_mutator->SetPrimaryAccount(account_info.account_id));
  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager->HasPrimaryAccountWithRefreshToken());

  EXPECT_EQ(identity_manager->GetPrimaryAccountId(), account_info.account_id);
  EXPECT_EQ(identity_manager->GetPrimaryAccountInfo().email,
            kPrimaryAccountEmail);

  if (auth_expection == AuthExpectation::kAuthError) {
    // Set primary account to have authentication error.
    SetRefreshTokenForPrimaryAccount(identity_manager);
    identity::UpdatePersistentErrorOfRefreshTokenForAccount(
        identity_manager, account_info.account_id,
        GoogleServiceAuthError(
            GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  }

  // Additionally, ClearPrimaryAccount_* tests also need a secondary account.
  AccountInfo secondary_account_info =
      environment.MakeAccountAvailable(kAnotherAccountEmail);
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      secondary_account_info.account_id));

  // Grab this before clearing for token checks below.
  auto former_primary_account = identity_manager->GetPrimaryAccountInfo();

  // Make sure we exit the run loop.
  base::RunLoop run_loop;
  PrimaryAccountClearedCallback primary_account_cleared_callback =
      base::BindRepeating([](base::RepeatingClosure quit_closure,
                             const CoreAccountInfo&) { quit_closure.Run(); },
                          run_loop.QuitClosure());

  // Track Observer token removal notification.
  base::flat_set<std::string> observed_removals;
  RefreshTokenRemovedCallback refresh_token_removed_callback =
      base::BindRepeating(
          [](base::flat_set<std::string>* observed_removals,
             const std::string& removed_account) {
            observed_removals->insert(removed_account);
          },
          &observed_removals);

  ClearPrimaryAccountTestObserver scoped_observer(
      identity_manager, primary_account_cleared_callback,
      refresh_token_removed_callback);

  primary_account_mutator->ClearPrimaryAccount(
      account_action, signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC);
  run_loop.Run();

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  // NOTE: IdentityManager _may_ still possess this token (see switch below),
  // but it is no longer considered part of the primary account.
  EXPECT_FALSE(identity_manager->HasPrimaryAccountWithRefreshToken());

  switch (account_expectation) {
    case RemoveAccountExpectation::kKeepAll:
      EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
          former_primary_account.account_id));
      EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
          secondary_account_info.account_id));
      EXPECT_TRUE(observed_removals.empty());
      break;
    case RemoveAccountExpectation::kRemovePrimary:
      EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
          former_primary_account.account_id));
      EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
          secondary_account_info.account_id));
      EXPECT_TRUE(base::ContainsKey(observed_removals,
                                    former_primary_account.account_id.id));
      EXPECT_FALSE(base::ContainsKey(observed_removals,
                                     secondary_account_info.account_id.id));
      break;
    case RemoveAccountExpectation::kRemoveAll:
      EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
          former_primary_account.account_id));
      EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
          secondary_account_info.account_id));
      EXPECT_TRUE(base::ContainsKey(observed_removals,
                                    former_primary_account.account_id.id));
      EXPECT_TRUE(base::ContainsKey(observed_removals,
                                    secondary_account_info.account_id.id));
      break;
  }
}

}  // namespace

using PrimaryAccountMutatorTest = PlatformTest;

// Checks that the method to control whether setting the primary account is
// working correctly and that the setting is respected by SetPrimaryAccount().
TEST_F(PrimaryAccountMutatorTest, SetSettingPrimaryAccountAllowed) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  primary_account_mutator->SetSettingPrimaryAccountAllowed(false);
  EXPECT_FALSE(primary_account_mutator->IsSettingPrimaryAccountAllowed());

  primary_account_mutator->SetSettingPrimaryAccountAllowed(true);
  EXPECT_TRUE(primary_account_mutator->IsSettingPrimaryAccountAllowed());
}

// Checks that setting the primary account works.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  AccountInfo account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(environment.identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(
      primary_account_mutator->SetPrimaryAccount(account_info.account_id));

  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_EQ(identity_manager->GetPrimaryAccountId(), account_info.account_id);
}

// Checks that setting the primary account fails if the account is not known by
// the identity service.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_NoAccount) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->SetPrimaryAccount(kUnknownAccountId));
}

// Checks that setting the primary account fails if the account is unknown.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_UnknownAccount) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  AccountInfo account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->SetPrimaryAccount(kUnknownAccountId));
}

// Checks that trying to set the primary account fails when there is already a
// primary account.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_AlreadyHasPrimaryAccount) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  AccountInfo primary_account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);
  AccountInfo another_account_info =
      environment.MakeAccountAvailable(kAnotherAccountEmail);

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_TRUE(primary_account_mutator->SetPrimaryAccount(
      primary_account_info.account_id));

  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->SetPrimaryAccount(
      another_account_info.account_id));

  EXPECT_EQ(identity_manager->GetPrimaryAccountId(),
            primary_account_info.account_id);
}

// Checks that trying to set the primary account fails if setting the primary
// account is not allowed.
TEST_F(PrimaryAccountMutatorTest,
       SetPrimaryAccount_SettingPrimaryAccountForbidden) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  AccountInfo primary_account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  primary_account_mutator->SetSettingPrimaryAccountAllowed(false);
  EXPECT_FALSE(primary_account_mutator->IsSettingPrimaryAccountAllowed());

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->SetPrimaryAccount(
      primary_account_info.account_id));
}

TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_NotSignedIn) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  // Trying to signout an account that hasn't signed in first should fail.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));

  // Adding an account without signing in should yield similar a result.
  AccountInfo primary_account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));
}

TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_Default) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  // This test requires two accounts to be made available.
  AccountInfo primary_account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);
  AccountInfo other_account_info =
      environment.MakeAccountAvailable(kAnotherAccountEmail);

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      other_account_info.account_id));

  // Sign in the primary account to check ClearPrimaryAccount() later on.
  primary_account_mutator->SetPrimaryAccount(primary_account_info.account_id);
  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_EQ(identity_manager->GetPrimaryAccountId(),
            primary_account_info.account_id);

  EXPECT_TRUE(primary_account_mutator->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));

  // The underlying SigninManager in IdentityTestEnvironment will be created
  // with signin::AccountConsistencyMethod::kDisabled, which should result in
  // ClearPrimaryAccount() removing all the tokens.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      other_account_info.account_id));
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kKeepAll
// keep all tokens, independently of the account consistency method.
TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_KeepAll) {
  for (signin::AccountConsistencyMethod account_consistency_method :
       kTestedAccountConsistencyMethods) {
    RunClearPrimaryAccountTest(
        account_consistency_method,
        identity::PrimaryAccountMutator::ClearAccountsAction::kKeepAll,
        RemoveAccountExpectation::kKeepAll);
  }
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kRemoveAll
// remove all tokens, independently of the account consistency method.
TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_RemoveAll) {
  for (signin::AccountConsistencyMethod account_consistency_method :
       kTestedAccountConsistencyMethods) {
    RunClearPrimaryAccountTest(
        account_consistency_method,
        identity::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
        RemoveAccountExpectation::kRemoveAll);
  }
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kDefault
// and AccountConsistencyMethod::kDisabled (notably != kDice) removes all
// tokens.
TEST_F(PrimaryAccountMutatorTest,
       ClearPrimaryAccount_Default_DisabledConsistency) {
  RunClearPrimaryAccountTest(
      signin::AccountConsistencyMethod::kDisabled,
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      RemoveAccountExpectation::kRemoveAll);
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kDefault
// and AccountConsistencyMethod::kMirror (notably != kDice) removes all
// tokens.
TEST_F(PrimaryAccountMutatorTest,
       ClearPrimaryAccount_Default_MirrorConsistency) {
  RunClearPrimaryAccountTest(
      signin::AccountConsistencyMethod::kMirror,
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      RemoveAccountExpectation::kRemoveAll);
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kDefault
// and AccountConsistencyMethod::kDice keeps all accounts when the the primary
// account does not have an authentication error (see *_AuthError test).
TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_Default_DiceConsistency) {
  RunClearPrimaryAccountTest(
      signin::AccountConsistencyMethod::kDice,
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      RemoveAccountExpectation::kKeepAll);
}

// Test that ClearPrimaryAccount(...) with ClearAccountTokensAction::kDefault
// and AccountConsistencyMethod::kDice removes *only* the primary account
// due to it authentication error.
TEST_F(PrimaryAccountMutatorTest,
       ClearPrimaryAccount_Default_DiceConsistency_AuthError) {
  RunClearPrimaryAccountTest(
      signin::AccountConsistencyMethod::kDice,
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      RemoveAccountExpectation::kRemovePrimary, AuthExpectation::kAuthError);
}
