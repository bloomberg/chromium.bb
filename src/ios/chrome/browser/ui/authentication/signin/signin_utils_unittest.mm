// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

#import <UIKit/UIKit.h>

#include <memory>

#import "base/bind.h"
#import "base/version.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"
#import "ios/chrome/test/block_cleanup_test.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::Version;
using sync_preferences::PrefServiceMockFactory;
using sync_preferences::PrefServiceSyncable;
using user_prefs::PrefRegistrySyncable;
using web::WebTaskEnvironment;

namespace {

class SigninUtilsTest : public BlockCleanupTest {
 public:
  SigninUtilsTest() : version_("1.0") {
    SetSigninCurrentVersionForTesting(&version_);
  }

  ~SigninUtilsTest() override { SetSigninCurrentVersionForTesting(nullptr); }

  void SetUp() override {
    BlockCleanupTest::SetUp();
    TestChromeBrowserState::Builder builder;
    builder.SetPrefService(CreatePrefService());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    chrome_browser_state_ = builder.Build();
    ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
        ->AddIdentities(@[ @"foo", @"bar" ]);
    WebStateList* web_state_list = nullptr;
    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                             web_state_list);
  }

  void TearDown() override {
    NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
    [standardDefaults removeObjectForKey:kDisplayedSSORecallForMajorVersionKey];
    [standardDefaults removeObjectForKey:kLastShownAccountGaiaIdVersionKey];
    [standardDefaults removeObjectForKey:kSigninPromoViewDisplayCountKey];
    [standardDefaults synchronize];
    BlockCleanupTest::TearDown();
  }

  std::unique_ptr<PrefServiceSyncable> CreatePrefService() {
    PrefServiceMockFactory factory;
    scoped_refptr<PrefRegistrySyncable> registry(new PrefRegistrySyncable);
    std::unique_ptr<PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    RegisterBrowserStatePrefs(registry.get());
    return prefs;
  }

 protected:
  WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  base::Version version_;
};

// Should show the sign-in upgrade for the first time.
TEST_F(SigninUtilsTest, TestWillDisplay) {
  EXPECT_TRUE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Should not show the sign-in upgrade twice on the same version.
TEST_F(SigninUtilsTest, TestWillNotDisplaySameVersion) {
  SigninRecordVersionSeen();
  EXPECT_FALSE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Should not show the sign-in upgrade twice until to major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMinorVersion) {
  SigninRecordVersionSeen();
  // Set the future version to be one minor release ahead.
  Version version_1_1("1.1");
  SetSigninCurrentVersionForTesting(&version_1_1);
  EXPECT_FALSE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Should not show the sign-in upgrade twice until to major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayTwoMinorVersions) {
  SigninRecordVersionSeen();
  // Set the future version to be two minor releases ahead.
  Version version_1_2("1.2");
  SetSigninCurrentVersionForTesting(&version_1_2);
  EXPECT_FALSE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Should not show the sign-in upgrade twice until to major version after.
TEST_F(SigninUtilsTest, TestWillNotDisplayOneMajorVersion) {
  SigninRecordVersionSeen();
  // Set the future version to be one major release ahead.
  Version version_2_0("2.0");
  SetSigninCurrentVersionForTesting(&version_2_0);
  EXPECT_FALSE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Should show the sign-in upgrade a second time, 2 version after.
TEST_F(SigninUtilsTest, TestWillDisplayTwoMajorVersions) {
  SigninRecordVersionSeen();
  // Set the future version to be two major releases ahead.
  Version version_3_0("3.0");
  SetSigninCurrentVersionForTesting(&version_3_0);
  EXPECT_TRUE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillShowTwoTimesOnly) {
  SigninRecordVersionSeen();
  Version version_3_0("3.0");
  SetSigninCurrentVersionForTesting(&version_3_0);
  SigninRecordVersionSeen();
  Version version_5_0("5.0");
  SetSigninCurrentVersionForTesting(&version_5_0);
  EXPECT_FALSE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Add new account.
// Expected: should show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillShowForNewAccountAdded) {
  SigninRecordVersionSeen();
  Version version_3_0("3.0");
  SetSigninCurrentVersionForTesting(&version_3_0);
  SigninRecordVersionSeen();
  Version version_5_0("5.0");
  SetSigninCurrentVersionForTesting(&version_5_0);
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo1" ]);
  EXPECT_TRUE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Add new account.
// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 5.0.
// Remove previous account.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillNotShowWithAccountRemoved) {
  NSString* newAccountGaiaId = @"foo1";
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ newAccountGaiaId ]);
  SigninRecordVersionSeen();
  Version version_3_0("3.0");
  SetSigninCurrentVersionForTesting(&version_3_0);
  SigninRecordVersionSeen();
  Version version_5_0("5.0");
  SetSigninCurrentVersionForTesting(&version_5_0);
  NSArray* allIdentities =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
          ->GetAllIdentities();
  ChromeIdentity* foo1Identity = nil;
  for (ChromeIdentity* identity in allIdentities) {
    if ([identity.userFullName isEqualToString:newAccountGaiaId]) {
      ASSERT_EQ(nil, foo1Identity);
      foo1Identity = identity;
    }
  }
  ASSERT_NE(nil, foo1Identity);
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->RemoveIdentity(foo1Identity);
  EXPECT_FALSE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Show the sign-in upgrade on version 1.0.
// Show the sign-in upgrade on version 3.0.
// Move to version 4.0.
// Add an account.
// Expected: should not show the sign-in upgrade.
TEST_F(SigninUtilsTest, TestWillNotShowNewAccountUntilTwoVersion) {
  SigninRecordVersionSeen();
  Version version_3_0("3.0");
  SetSigninCurrentVersionForTesting(&version_3_0);
  SigninRecordVersionSeen();
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo1" ]);
  Version version_4_0("4.0");
  SetSigninCurrentVersionForTesting(&version_4_0);
  EXPECT_FALSE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

// Show the sign-in upgrade on version 1.0.
// Move to version 2.0.
// Add an account.
// Expected: should not show the sign-in upgrade (only display every 2
// versions).
TEST_F(SigninUtilsTest, TestWillNotShowNewAccountUntilTwoVersionBis) {
  SigninRecordVersionSeen();
  Version version_2_0("2.0");
  SetSigninCurrentVersionForTesting(&version_2_0);
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->AddIdentities(@[ @"foo1" ]);
  EXPECT_FALSE(
      SigninShouldPresentUserSigninUpgrade(chrome_browser_state_.get()));
}

}  // namespace
