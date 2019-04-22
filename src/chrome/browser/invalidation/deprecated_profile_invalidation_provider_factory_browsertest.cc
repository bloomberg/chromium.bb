// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/invalidation/deprecated_profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/public/invalidation_service.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class DeprecatedProfileInvalidationProviderFactoryTestBase
    : public InProcessBrowserTest {
 protected:
  DeprecatedProfileInvalidationProviderFactoryTestBase();
  ~DeprecatedProfileInvalidationProviderFactoryTestBase() override;

  bool CanConstructProfileInvalidationProvider(Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DeprecatedProfileInvalidationProviderFactoryTestBase);
};

DeprecatedProfileInvalidationProviderFactoryTestBase::
    DeprecatedProfileInvalidationProviderFactoryTestBase() {}

DeprecatedProfileInvalidationProviderFactoryTestBase::
    ~DeprecatedProfileInvalidationProviderFactoryTestBase() {}

bool DeprecatedProfileInvalidationProviderFactoryTestBase::
    CanConstructProfileInvalidationProvider(Profile* profile) {
  return static_cast<bool>(
      DeprecatedProfileInvalidationProviderFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, false));
}

class DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest
    : public DeprecatedProfileInvalidationProviderFactoryTestBase {
 protected:
  DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest();
  ~DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest()
      override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest);
};

DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest::
    DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest() {}

DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest::
    ~DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest() {}

void DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest::
    SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
}

// Verify that no InvalidationService is instantiated for the login profile on
// the login screen.
IN_PROC_BROWSER_TEST_F(
    DeprecatedProfileInvalidationProviderFactoryLoginScreenBrowserTest,
    NoInvalidationService) {
  Profile* login_profile =
      chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
  EXPECT_FALSE(CanConstructProfileInvalidationProvider(login_profile));
}

class DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest
    : public DeprecatedProfileInvalidationProviderFactoryTestBase {
 protected:
  DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest();
  ~DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(
      DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest);
};

DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest::
    DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest() {}

DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest::
    ~DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest() {}

void DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest::
    SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kGuestSession);
  command_line->AppendSwitch(::switches::kIncognito);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  command_line->AppendSwitchASCII(
      chromeos::switches::kLoginUser,
      user_manager::GuestAccountId().GetUserEmail());
}

// Verify that no InvalidationService is instantiated for the login profile or
// the guest profile while a guest session is in progress.
IN_PROC_BROWSER_TEST_F(
    DeprecatedProfileInvalidationProviderFactoryGuestBrowserTest,
    NoInvalidationService) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
  Profile* guest_profile =
      chromeos::ProfileHelper::Get()
          ->GetProfileByUserUnsafe(user_manager->GetActiveUser())
          ->GetOriginalProfile();
  Profile* login_profile =
      chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
  EXPECT_FALSE(CanConstructProfileInvalidationProvider(guest_profile));
  EXPECT_FALSE(CanConstructProfileInvalidationProvider(login_profile));
}

}  // namespace invalidation
