// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_mutator.h"

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/platform_test.h"

namespace {
const char kUnknownAccountId[] = "{unknown account id}";
const char kPrimaryAccountEmail[] = "primary.account@example.com";
const char kAnotherAccountEmail[] = "another.account@example.com";
const char kRefreshToken[] = "refresh_token";
const char kPassword[] = "password";
}  // namespace

class PrimaryAccountMutatorTest : public PlatformTest {
 public:
  identity::IdentityTestEnvironment* environment() { return &environment_; }

  identity::IdentityManager* identity_manager() {
    return environment()->identity_manager();
  }

  identity::PrimaryAccountMutator* primary_account_mutator() {
    return identity_manager()->GetPrimaryAccountMutator();
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  identity::IdentityTestEnvironment environment_;
};

// Checks that the method to control whether setting the primary account is
// working correctly and that the setting is respected by SetPrimaryAccount().
TEST_F(PrimaryAccountMutatorTest, SetSettingPrimaryAccountAllowed) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  primary_account_mutator()->SetSettingPrimaryAccountAllowed(false);
  EXPECT_FALSE(primary_account_mutator()->IsSettingPrimaryAccountAllowed());

  primary_account_mutator()->SetSettingPrimaryAccountAllowed(true);
  EXPECT_TRUE(primary_account_mutator()->IsSettingPrimaryAccountAllowed());
}

// Checks that setting the primary account works.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  AccountInfo account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(
      primary_account_mutator()->SetPrimaryAccount(account_info.account_id));

  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(), account_info.account_id);
}

// Checks that setting the primary account fails if the account is not known by
// the identity service.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_NoAccount) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator()->SetPrimaryAccount(kUnknownAccountId));
}

// Checks that setting the primary account fails if the account is unknown.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_UnknownAccount) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  AccountInfo account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator()->SetPrimaryAccount(kUnknownAccountId));
}

// Checks that trying to set the primary account fails when there is already a
// primary account.
TEST_F(PrimaryAccountMutatorTest, SetPrimaryAccount_AlreadyHasPrimaryAccount) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  AccountInfo primary_account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);
  AccountInfo another_account_info =
      environment()->MakeAccountAvailable(kAnotherAccountEmail);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(primary_account_mutator()->SetPrimaryAccount(
      primary_account_info.account_id));

  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator()->SetPrimaryAccount(
      another_account_info.account_id));

  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(),
            primary_account_info.account_id);
}

// Checks that trying to set the primary account fails if setting the primary
// account is not allowed.
TEST_F(PrimaryAccountMutatorTest,
       SetPrimaryAccount_SettingPrimaryAccountForbidden) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  AccountInfo primary_account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);

  primary_account_mutator()->SetSettingPrimaryAccountAllowed(false);
  EXPECT_FALSE(primary_account_mutator()->IsSettingPrimaryAccountAllowed());

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator()->SetPrimaryAccount(
      primary_account_info.account_id));
}

TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_NotSignedIn) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  // Trying to signout an account that hasn't signed in first should fail.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator()->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));

  // Adding an account without signing in should yield similar a result.
  AccountInfo primary_account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_mutator()->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));
}

TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_Default) {
  if (!primary_account_mutator())
    return;

  // This test requires two accounts to be made available.
  AccountInfo primary_account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);
  AccountInfo other_account_info =
      environment()->MakeAccountAvailable(kAnotherAccountEmail);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      other_account_info.account_id));

  // Sign in the primary account to check ClearPrimaryAccount() later on.
  primary_account_mutator()->SetPrimaryAccount(primary_account_info.account_id);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(),
            primary_account_info.account_id);

  EXPECT_TRUE(primary_account_mutator()->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kDefault,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));

  // The underlying SigninManager in IdentityTestEnvironment (FakeSigninManager)
  // will be created with signin::AccountConsistencyMethod::kDisabled, which
  // should result in ClearPrimaryAccount() removing all the tokens.
  // Other possible combinations are covered by IdentityManagerTests unit tests.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      other_account_info.account_id));
}

TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_KeepAll) {
  if (!primary_account_mutator())
    return;

  // This test requires two accounts to be made available.
  AccountInfo primary_account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);
  AccountInfo other_account_info =
      environment()->MakeAccountAvailable(kAnotherAccountEmail);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      other_account_info.account_id));

  // Sign in the primary account to check ClearPrimaryAccount() later on.
  primary_account_mutator()->SetPrimaryAccount(primary_account_info.account_id);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(),
            primary_account_info.account_id);

  EXPECT_TRUE(primary_account_mutator()->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kKeepAll,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));

  // ClearPrimaryAccount() should keep all the accounts available in this case.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      other_account_info.account_id));
}

TEST_F(PrimaryAccountMutatorTest, ClearPrimaryAccount_RemoveAll) {
  if (!primary_account_mutator())
    return;

  // This test requires two accounts to be made available.
  AccountInfo primary_account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);
  AccountInfo other_account_info =
      environment()->MakeAccountAvailable(kAnotherAccountEmail);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      other_account_info.account_id));

  // Sign in the primary account to check ClearPrimaryAccount() later on.
  primary_account_mutator()->SetPrimaryAccount(primary_account_info.account_id);
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(),
            primary_account_info.account_id);

  EXPECT_TRUE(primary_account_mutator()->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kRemoveAll,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));

  // ClearPrimaryAccount() should remove all the accounts in this case.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      other_account_info.account_id));
}

// Checks that checking whether an authentication process is in progress reports
// true before starting and after successfully completing the signin process.
TEST_F(PrimaryAccountMutatorTest, SigninWithRefreshToken) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  // We'll sign in the same account twice, using SetPrimaryAccount() and
  // LegacyStartSigninWithRefreshTokenForPrimaryAccount(), and check that
  // in both cases the end result is the same.
  AccountInfo account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(
      primary_account_mutator()->SetPrimaryAccount(account_info.account_id));

  const std::string primary_account_id_1 =
      identity_manager()->GetPrimaryAccountId();

  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_id_1.empty());
  EXPECT_EQ(primary_account_id_1, account_info.account_id);

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(primary_account_id_1));
  EXPECT_FALSE(identity_manager()->GetPrimaryAccountInfo().email.empty());

  // Sign out the account to try to sign in again with the other mechanism, but
  // using kKeepAll so we can use the same account we made available before.
  EXPECT_TRUE(primary_account_mutator()->ClearPrimaryAccount(
      identity::PrimaryAccountMutator::ClearAccountsAction::kKeepAll,
      signin_metrics::SIGNOUT_TEST,
      signin_metrics::SignoutDelete::IGNORE_METRIC));

  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_TRUE(identity_manager()->GetPrimaryAccountId().empty());
  EXPECT_TRUE(identity_manager()->GetPrimaryAccountInfo().email.empty());

  // Start a signin process for the account and complete it right away to check
  // whether we end up with a similar result than with SetPrimaryAccount().
  std::string signed_account_refresh_token;
  primary_account_mutator()->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      kRefreshToken, account_info.gaia, account_info.email, kPassword,
      base::BindOnce(
          [](std::string* out_refresh_token, const std::string& refresh_token) {
            *out_refresh_token = refresh_token;
          },
          base::Unretained(&signed_account_refresh_token)));

  primary_account_mutator()->LegacyCompletePendingPrimaryAccountSignin();

  // The refresh token assigned to the account should match the one passed.
  EXPECT_EQ(signed_account_refresh_token, kRefreshToken);

  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_id_1.empty());
  EXPECT_EQ(primary_account_id_1, account_info.account_id);

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(primary_account_id_1));
  EXPECT_FALSE(identity_manager()->GetPrimaryAccountInfo().email.empty());

  const std::string primary_account_id_2 =
      identity_manager()->GetPrimaryAccountId();

  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(primary_account_id_2.empty());
  EXPECT_EQ(primary_account_id_2, account_info.account_id);

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(primary_account_id_2));
  EXPECT_FALSE(identity_manager()->GetPrimaryAccountInfo().email.empty());

  // Information retrieved via the IdentityManager now for the current primary
  // account should match the data of the account we signed in before.
  EXPECT_EQ(primary_account_id_1, primary_account_id_2);
}

// Checks that checking whether an authentication process is in progress reports
// true before starting and after successfully completing the signin process.
TEST_F(PrimaryAccountMutatorTest, AuthInProgress_SigninCompleted) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  AccountInfo account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);

  // Account available in the tracker service but still not authenticated means
  // there's neither a primary account nor an authentication process ongoing.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(
      primary_account_mutator()->LegacyIsPrimaryAccountAuthInProgress());

  // Start a signin process for the account we just made available and check
  // that it's reported to be in progress before the process is completed.
  base::RunLoop run_loop;
  std::string signed_account_refresh_token;
  primary_account_mutator()->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      kRefreshToken, account_info.gaia, account_info.email, kPassword,
      base::BindOnce(
          [](std::string* out_refresh_token, const std::string& refresh_token) {
            *out_refresh_token = refresh_token;
          },
          base::Unretained(&signed_account_refresh_token)));

  EXPECT_TRUE(
      primary_account_mutator()->LegacyIsPrimaryAccountAuthInProgress());

  AccountInfo auth_in_progress_account_info =
      primary_account_mutator()->LegacyPrimaryAccountForAuthInProgress();

  // The data from the AccountInfo related to the authentication process still
  // in progress should match the data of the account being signed in.
  EXPECT_EQ(auth_in_progress_account_info.account_id, account_info.account_id);
  EXPECT_EQ(auth_in_progress_account_info.gaia, account_info.gaia);
  EXPECT_EQ(auth_in_progress_account_info.email, account_info.email);

  // Finally, complete the signin process so that we can do further checks.
  primary_account_mutator()->LegacyCompletePendingPrimaryAccountSignin();
  run_loop.RunUntilIdle();

  // The refresh token assigned to the account should match the one passed.
  EXPECT_EQ(signed_account_refresh_token, kRefreshToken);

  // An account has been authenticated now, so there should be a primary account
  // authenticated and no authentication process reported as in progress now.
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(
      primary_account_mutator()->LegacyIsPrimaryAccountAuthInProgress());

  // Information retrieved via the IdentityManager now for the current primary
  // account should match the data of the account being signed in.
  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(), account_info.account_id);
  AccountInfo identity_manager_account_info =
      identity_manager()->GetPrimaryAccountInfo();
  EXPECT_EQ(identity_manager_account_info.account_id, account_info.account_id);
  EXPECT_EQ(identity_manager_account_info.gaia, account_info.gaia);
  EXPECT_EQ(identity_manager_account_info.email, account_info.email);
}

// Checks that checking whether an authentication process is in progress reports
// true before starting and after cancelling and ongoing signin process.
TEST_F(PrimaryAccountMutatorTest, AuthInProgress_SigninCancelled) {
  // Abort the test if the current platform does not support mutation of the
  // primary account (the returned PrimaryAccountMutator* will be null).
  if (!primary_account_mutator())
    return;

  AccountInfo account_info =
      environment()->MakeAccountAvailable(kPrimaryAccountEmail);

  // Account available in the tracker service but still not authenticated means
  // there's neither a primary account nor an authentication process ongoing.
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(
      primary_account_mutator()->LegacyIsPrimaryAccountAuthInProgress());

  // Start a signin process for the account we just made available and check
  // that it's reported to be in progress before the process is completed.
  base::RunLoop run_loop;
  std::string signed_account_refresh_token;
  primary_account_mutator()->LegacyStartSigninWithRefreshTokenForPrimaryAccount(
      kRefreshToken, account_info.gaia, account_info.email, kPassword,
      base::BindOnce(
          [](std::string* out_refresh_token, const std::string& refresh_token) {
            *out_refresh_token = refresh_token;
          },
          base::Unretained(&signed_account_refresh_token)));

  EXPECT_TRUE(
      primary_account_mutator()->LegacyIsPrimaryAccountAuthInProgress());

  AccountInfo auth_in_progress_account_info =
      primary_account_mutator()->LegacyPrimaryAccountForAuthInProgress();

  // The data from the AccountInfo related to the authentication process still
  // in progress should match the data of the account being signed in.
  EXPECT_EQ(auth_in_progress_account_info.account_id, account_info.account_id);
  EXPECT_EQ(auth_in_progress_account_info.gaia, account_info.gaia);
  EXPECT_EQ(auth_in_progress_account_info.email, account_info.email);

  // Now cancel the signin process (by attempting to clear the primary account
  // we were trying to sign in so far), so that we can do further checks.
  EXPECT_TRUE(primary_account_mutator()->ClearPrimaryAccount(
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
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken());
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(
      primary_account_mutator()->LegacyIsPrimaryAccountAuthInProgress());

  // Information retrieved via the IdentityManager confirms the cancelation.
  EXPECT_EQ(identity_manager()->GetPrimaryAccountId(), std::string());
  EXPECT_TRUE(identity_manager()->GetPrimaryAccountInfo().IsEmpty());
}
