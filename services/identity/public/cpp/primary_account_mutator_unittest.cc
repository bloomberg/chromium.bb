// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/primary_account_mutator.h"

#include "base/test/scoped_task_environment.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "testing/platform_test.h"

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
