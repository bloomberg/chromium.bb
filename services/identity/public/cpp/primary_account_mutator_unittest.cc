// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_mutator.h"

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
const char kRefreshToken[] = "refresh_token";
const char kPassword[] = "password";

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
    base::RepeatingCallback<void(const AccountInfo&)>;

// This callback will be invoked every time the IdentityManager::Observer
// method OnPrimaryAccountSigninFailed is invoked. The parameter will be
// a reference to the authentication error.
using PrimaryAccountSigninFailedCallback =
    base::RepeatingCallback<void(const GoogleServiceAuthError&)>;

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
      PrimaryAccountSigninFailedCallback on_primary_account_signin_failed,
      RefreshTokenRemovedCallback on_refresh_token_removed)
      : on_primary_account_cleared_(std::move(on_primary_account_cleared)),
        on_primary_account_signin_failed_(
            std::move(on_primary_account_signin_failed)),
        on_refresh_token_removed_(std::move(on_refresh_token_removed)),
        scoped_observer_(this) {
    DCHECK(on_primary_account_cleared_);
    DCHECK(on_primary_account_signin_failed_);
    DCHECK(on_refresh_token_removed_);
    scoped_observer_.Add(identity_manager);
  }

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountCleared(const AccountInfo& account_info) override {
    on_primary_account_cleared_.Run(account_info);
  }

  void OnPrimaryAccountSigninFailed(
      const GoogleServiceAuthError& error) override {
    on_primary_account_signin_failed_.Run(error);
  }

  void OnRefreshTokenRemovedForAccount(const std::string& account_id) override {
    on_refresh_token_removed_.Run(account_id);
  }

 private:
  PrimaryAccountClearedCallback on_primary_account_cleared_;
  PrimaryAccountSigninFailedCallback on_primary_account_signin_failed_;
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
                             const AccountInfo&) { quit_closure.Run(); },
                          run_loop.QuitClosure());

  // Authentication error should not occur.
  PrimaryAccountSigninFailedCallback primary_account_signin_failed_callback =
      base::BindRepeating([](const GoogleServiceAuthError&) {
        FAIL() << "auth should not fail";
      });

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
      primary_account_signin_failed_callback, refresh_token_removed_callback);

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
                                    former_primary_account.account_id));
      EXPECT_FALSE(base::ContainsKey(observed_removals,
                                     secondary_account_info.account_id));
      break;
    case RemoveAccountExpectation::kRemoveAll:
      EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
          former_primary_account.account_id));
      EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
          secondary_account_info.account_id));
      EXPECT_TRUE(base::ContainsKey(observed_removals,
                                    former_primary_account.account_id));
      EXPECT_TRUE(base::ContainsKey(observed_removals,
                                    secondary_account_info.account_id));
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

  // The underlying SigninManager in IdentityTestEnvironment (FakeSigninManager)
  // will be created with signin::AccountConsistencyMethod::kDisabled, which
  // should result in ClearPrimaryAccount() removing all the tokens.
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

// Test that ClearPrimaryAccount(...) with authentication in progress notifies
// Observers that sign-in is canceled and does not remove any tokens.
TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_AuthInProgress) {
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
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));

  // Account available in the tracker service but still not authenticated means
  // there's neither a primary account nor an authentication process ongoing.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  // Add a secondary account to verify that its refresh token survives the
  // call to ClearPrimaryAccount(...) below.
  AccountInfo secondary_account_info =
      MakeAccountAvailable(identity_manager, kAnotherAccountEmail);
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      secondary_account_info.account_id));

  // Start a signin process for the account we just made available and check
  // that it's reported to be in progress before the process is completed.
  base::RunLoop run_loop;
  std::string signed_account_refresh_token;
  primary_account_mutator->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      kRefreshToken, account_info.gaia, account_info.email, kPassword,
      base::BindOnce(
          [](std::string* out_refresh_token, const std::string& refresh_token) {
            *out_refresh_token = refresh_token;
          },
          base::Unretained(&signed_account_refresh_token)));

  EXPECT_TRUE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  AccountInfo auth_in_progress_account_info =
      primary_account_mutator->LegacyPrimaryAccountForAuthInProgress();

  // No primary account to "clear", so no callback.
  PrimaryAccountClearedCallback primary_account_cleared_callback =
      base::BindRepeating([](const AccountInfo&) {
        FAIL() << "no primary account is set, so nothing should be cleared";
      });

  // Capture the authentication error and make sure we exit the run loop.
  GoogleServiceAuthError captured_auth_error;
  PrimaryAccountSigninFailedCallback primary_account_signin_failed_callback =
      base::BindRepeating(
          [](base::RepeatingClosure quit_closure,
             GoogleServiceAuthError* out_auth_error,
             const GoogleServiceAuthError& auth_error) {
            *out_auth_error = auth_error;
            quit_closure.Run();
          },
          run_loop.QuitClosure(), &captured_auth_error);

  // Observer should not be notified of any token removals.
  RefreshTokenRemovedCallback refresh_token_removed_callback =
      base::BindRepeating([](const std::string& removed_account) {
        FAIL() << "no token removal should happen";
      });

  ClearPrimaryAccountTestObserver scoped_observer(
      identity_manager, primary_account_cleared_callback,
      primary_account_signin_failed_callback, refresh_token_removed_callback);

  EXPECT_TRUE(primary_account_mutator->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));
  run_loop.Run();

  // Verify in-progress authentication was canceled.
  EXPECT_EQ(captured_auth_error.state(),
            GoogleServiceAuthError::State::REQUEST_CANCELED);
  EXPECT_FALSE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  // We didn't have a primary account to start with, we shouldn't have one now
  // either.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager->HasPrimaryAccountWithRefreshToken());

  // Secondary account token still exists.
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
}

// Checks that checking whether an authentication process is in progress reports
// true before starting and after successfully completing the signin process.
TEST_F(PrimaryAccountMutatorTest, SigninWithRefreshToken) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  // We'll sign in the same account twice, using SetPrimaryAccount() and
  // LegacyStartSigninWithRefreshTokenForPrimaryAccount(), and check that
  // in both cases the end result is the same.
  AccountInfo account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_TRUE(
      primary_account_mutator->SetPrimaryAccount(account_info.account_id));

  const std::string primary_account_id_1 =
      identity_manager->GetPrimaryAccountId();

  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_id_1.empty());
  EXPECT_EQ(primary_account_id_1, account_info.account_id);

  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account_id_1));
  EXPECT_FALSE(identity_manager->GetPrimaryAccountInfo().email.empty());

  // Sign out the account to try to sign in again with the other mechanism, but
  // using kKeepAll so we can use the same account we made available before.
  EXPECT_TRUE(primary_account_mutator->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kKeepAll,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));

  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager->GetPrimaryAccountId().empty());
  EXPECT_TRUE(identity_manager->GetPrimaryAccountInfo().email.empty());

  // Start a signin process for the account and complete it right away to check
  // whether we end up with a similar result than with SetPrimaryAccount().
  std::string signed_account_refresh_token;
  primary_account_mutator->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      kRefreshToken, account_info.gaia, account_info.email, kPassword,
      base::BindOnce(
          [](std::string* out_refresh_token, const std::string& refresh_token) {
            *out_refresh_token = refresh_token;
          },
          base::Unretained(&signed_account_refresh_token)));

  primary_account_mutator->LegacyCompletePendingPrimaryAccountSignin();

  // The refresh token assigned to the account should match the one passed.
  EXPECT_EQ(signed_account_refresh_token, kRefreshToken);

  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_id_1.empty());
  EXPECT_EQ(primary_account_id_1, account_info.account_id);

  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account_id_1));
  EXPECT_FALSE(identity_manager->GetPrimaryAccountInfo().email.empty());

  const std::string primary_account_id_2 =
      identity_manager->GetPrimaryAccountId();

  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_id_2.empty());
  EXPECT_EQ(primary_account_id_2, account_info.account_id);

  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(primary_account_id_2));
  EXPECT_FALSE(identity_manager->GetPrimaryAccountInfo().email.empty());

  // Information retrieved via the IdentityManager now for the current primary
  // account should match the data of the account we signed in before.
  EXPECT_EQ(primary_account_id_1, primary_account_id_2);
}

// Checks that checking whether an authentication process is in progress reports
// true before starting and after successfully completing the signin process.
TEST_F(PrimaryAccountMutatorTest, AuthInProgress_SigninCompleted) {
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

  // Account available in the tracker service but still not authenticated means
  // there's neither a primary account nor an authentication process ongoing.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  // Start a signin process for the account we just made available and check
  // that it's reported to be in progress before the process is completed.
  base::RunLoop run_loop;
  std::string signed_account_refresh_token;
  primary_account_mutator->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      kRefreshToken, account_info.gaia, account_info.email, kPassword,
      base::BindOnce(
          [](std::string* out_refresh_token, const std::string& refresh_token) {
            *out_refresh_token = refresh_token;
          },
          base::Unretained(&signed_account_refresh_token)));

  EXPECT_TRUE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  AccountInfo auth_in_progress_account_info =
      primary_account_mutator->LegacyPrimaryAccountForAuthInProgress();

  // The data from the AccountInfo related to the authentication process still
  // in progress should match the data of the account being signed in.
  EXPECT_EQ(auth_in_progress_account_info.account_id, account_info.account_id);
  EXPECT_EQ(auth_in_progress_account_info.gaia, account_info.gaia);
  EXPECT_EQ(auth_in_progress_account_info.email, account_info.email);

  // Finally, complete the signin process so that we can do further checks.
  primary_account_mutator->LegacyCompletePendingPrimaryAccountSignin();
  run_loop.RunUntilIdle();

  // The refresh token assigned to the account should match the one passed.
  EXPECT_EQ(signed_account_refresh_token, kRefreshToken);

  // An account has been authenticated now, so there should be a primary account
  // authenticated and no authentication process reported as in progress now.
  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  // Information retrieved via the IdentityManager now for the current primary
  // account should match the data of the account being signed in.
  EXPECT_EQ(identity_manager->GetPrimaryAccountId(), account_info.account_id);
  AccountInfo identity_manager_account_info =
      identity_manager->GetPrimaryAccountInfo();
  EXPECT_EQ(identity_manager_account_info.account_id, account_info.account_id);
  EXPECT_EQ(identity_manager_account_info.gaia, account_info.gaia);
  EXPECT_EQ(identity_manager_account_info.email, account_info.email);
}

// Checks that checking whether an authentication process is in progress reports
// true before starting and after cancelling and ongoing signin process.
TEST_F(PrimaryAccountMutatorTest, AuthInProgress_SigninCancelled) {
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

  // Account available in the tracker service but still not authenticated means
  // there's neither a primary account nor an authentication process ongoing.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  // Start a signin process for the account we just made available and check
  // that it's reported to be in progress before the process is completed.
  base::RunLoop run_loop;
  std::string signed_account_refresh_token;
  primary_account_mutator->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      kRefreshToken, account_info.gaia, account_info.email, kPassword,
      base::BindOnce(
          [](std::string* out_refresh_token, const std::string& refresh_token) {
            *out_refresh_token = refresh_token;
          },
          base::Unretained(&signed_account_refresh_token)));

  EXPECT_TRUE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  AccountInfo auth_in_progress_account_info =
      primary_account_mutator->LegacyPrimaryAccountForAuthInProgress();

  // The data from the AccountInfo related to the authentication process still
  // in progress should match the data of the account being signed in.
  EXPECT_EQ(auth_in_progress_account_info.account_id, account_info.account_id);
  EXPECT_EQ(auth_in_progress_account_info.gaia, account_info.gaia);
  EXPECT_EQ(auth_in_progress_account_info.email, account_info.email);

  // Now cancel the signin process (by attempting to clear the primary account
  // we were trying to sign in so far), so that we can do further checks.
  EXPECT_TRUE(primary_account_mutator->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));
  run_loop.RunUntilIdle();

  // The refresh token assigned to the account should match the one passed.
  EXPECT_EQ(signed_account_refresh_token, kRefreshToken);

  // The request has been cancelled, so there should not be a primary account
  // signed in, the refresh we just received should not be valid for the primary
  // account (even if it's been fetched and stored for the account already) and
  // no authentication process reported as in progress now.
  EXPECT_FALSE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager->HasPrimaryAccountWithRefreshToken());
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  // Information retrieved via the IdentityManager confirms the cancelation.
  EXPECT_EQ(identity_manager->GetPrimaryAccountId(), std::string());
  EXPECT_TRUE(identity_manager->GetPrimaryAccountInfo().IsEmpty());
}

// Checks that copying the credentials from another PrimaryAccountMutator works.
TEST_F(PrimaryAccountMutatorTest, CopyCredentialsFrom) {
  base::test::ScopedTaskEnvironment task_environment;
  identity::IdentityTestEnvironment environment;

  identity::IdentityManager* identity_manager = environment.identity_manager();
  identity::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();

  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator)
    return;

  // We will need another PrimaryAccountMutator to copy the credentials from the
  // one used previously and check that they match later on.
  identity::IdentityTestEnvironment other_environment;
  identity::IdentityManager* other_identity_manager =
      other_environment.identity_manager();
  identity::PrimaryAccountMutator* other_primary_account_mutator =
      other_identity_manager->GetPrimaryAccountMutator();

  AccountInfo account_info =
      environment.MakeAccountAvailable(kPrimaryAccountEmail);

  // Start a signin process for the account we just made available so that we
  // can check whether the credentials copied to another PrimaryAccountMutator.
  base::RunLoop run_loop;
  std::string signed_account_refresh_token;
  primary_account_mutator->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      kRefreshToken, account_info.gaia, account_info.email, kPassword,
      base::BindOnce(
          [](std::string* out_refresh_token, const std::string& refresh_token) {
            *out_refresh_token = refresh_token;
          },
          base::Unretained(&signed_account_refresh_token)));
  run_loop.RunUntilIdle();

  // The refresh token assigned to the account should match the one passed.
  EXPECT_EQ(signed_account_refresh_token, kRefreshToken);

  // This is a good moment to copy the credentials from one mutator to the other
  // since internal transient data hold by the SigninManager in this state will
  // be non-empty while the authentication process is ongoing (e.g. possibly
  // invalid account ID, Gaia ID and email), allowing us to compare values.
  base::RunLoop run_loop2;
  other_primary_account_mutator->LegacyCopyCredentialsFrom(
      *primary_account_mutator);
  run_loop2.RunUntilIdle();

  EXPECT_TRUE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());
  EXPECT_TRUE(
      other_primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  AccountInfo auth_in_progress_account_info =
      primary_account_mutator->LegacyPrimaryAccountForAuthInProgress();
  AccountInfo other_auth_in_progress_account_info =
      other_primary_account_mutator->LegacyPrimaryAccountForAuthInProgress();

  EXPECT_FALSE(auth_in_progress_account_info.IsEmpty());
  EXPECT_FALSE(other_auth_in_progress_account_info.IsEmpty());

  EXPECT_EQ(auth_in_progress_account_info.account_id,
            other_auth_in_progress_account_info.account_id);
  EXPECT_EQ(auth_in_progress_account_info.gaia,
            other_auth_in_progress_account_info.gaia);
  EXPECT_EQ(auth_in_progress_account_info.email,
            other_auth_in_progress_account_info.email);

  // Finally, complete the signin process so that we can do further checks.
  base::RunLoop run_loop3;
  primary_account_mutator->LegacyCompletePendingPrimaryAccountSignin();
  run_loop3.RunUntilIdle();

  // An account has been authenticated now, so there should be a primary account
  // authenticated and no authentication process reported as in progress now.
  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  // Query again the information for each of the two different environments now
  // that the original one has completed the authentication process and compare
  // them one more time: they should not match as the original one is no longer
  // in the middle of the authentication process.
  EXPECT_TRUE(identity_manager->HasPrimaryAccount());
  EXPECT_FALSE(other_identity_manager->HasPrimaryAccount());

  EXPECT_FALSE(primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());
  EXPECT_TRUE(
      other_primary_account_mutator->LegacyIsPrimaryAccountAuthInProgress());

  auth_in_progress_account_info =
      primary_account_mutator->LegacyPrimaryAccountForAuthInProgress();
  other_auth_in_progress_account_info =
      other_primary_account_mutator->LegacyPrimaryAccountForAuthInProgress();
  EXPECT_TRUE(auth_in_progress_account_info.IsEmpty());
  EXPECT_FALSE(other_auth_in_progress_account_info.IsEmpty());
}
