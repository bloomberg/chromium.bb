// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_mutator.h"

#include "base/test/scoped_task_environment.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/platform_test.h"

namespace {
const char kUnknownAccountId[] = "{unknown account id}";
const char kPrimaryAccountEmail[] = "primary.account@example.com";
const char kAnotherAccountEmail[] = "another.account@example.com";
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
