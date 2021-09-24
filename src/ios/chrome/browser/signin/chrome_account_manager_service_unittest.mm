// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_account_manager_service.h"

#include "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#include "ios/chrome/test/testing_application_context.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
FakeChromeIdentity* identity1 =
    [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                   gaiaID:@"foo1ID"
                                     name:@"Fake Foo 1"];
FakeChromeIdentity* identity2 =
    [FakeChromeIdentity identityWithEmail:@"foo2@google.com"
                                   gaiaID:@"foo2ID"
                                     name:@"Fake Foo 2"];
FakeChromeIdentity* identity3 =
    [FakeChromeIdentity identityWithEmail:@"foo3@chromium.com"
                                   gaiaID:@"foo3ID"
                                     name:@"Fake Foo 3"];
FakeChromeIdentity* identity4 =
    [FakeChromeIdentity identityWithEmail:@"foo4@chromium.com"
                                   gaiaID:@"foo3ID"
                                     name:@"Fake Foo 3"];
}  // namespace

class ChromeAccountManagerServiceTest : public PlatformTest {
 public:
  ChromeAccountManagerServiceTest() {
    TestChromeBrowserState::Builder builder;
    browser_state_ = builder.Build();

    account_manager_ = ChromeAccountManagerServiceFactory::GetForBrowserState(
        browser_state_.get());
    identity_service_ =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  // Adds identities to the identity service.
  void AddIdentities() {
    identity_service_->AddIdentity(identity1);
    identity_service_->AddIdentity(identity2);
    identity_service_->AddIdentity(identity3);
    identity_service_->AddIdentity(identity4);
  }

  // Sets a restricted pattern.
  void SetPattern(const std::string pattern) {
    base::ListValue allowed_patterns;
    allowed_patterns.Append(pattern);
    GetApplicationContext()->GetLocalState()->Set(
        prefs::kRestrictAccountsToPatterns, allowed_patterns);
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  ChromeAccountManagerService* account_manager_;
  ios::FakeChromeIdentityService* identity_service_;
};

// Tests to get identities when the restricted pattern is not set.
TEST_F(ChromeAccountManagerServiceTest, TestHasIdentities) {
  EXPECT_EQ(account_manager_->HasIdentities(), false);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 0);

  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 4);
}

// Tests to get identity when the restricted pattern matches only one identity.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithValidRestrictedPattern) {
  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);

  SetPattern("*gmail.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), false);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 1);

  SetPattern("foo2@google.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), false);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 1);
}

// Tests to get identities when the restricted pattern matches several
// identities.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentitiesWithValidRestrictedPattern) {
  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);

  SetPattern("*chromium.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), true);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 2);
}

// Tests to get identity when the restricted pattern doesn't match any identity.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithInvalidRestrictedPattern) {
  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);

  SetPattern("*none.com");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), false);
  EXPECT_EQ(account_manager_->HasIdentities(), false);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 0);
}

// Tests to get identity when all identities are matched by pattern.
TEST_F(ChromeAccountManagerServiceTest,
       TestGetIdentityWithAllInclusivePattern) {
  AddIdentities();
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);

  SetPattern("*");
  EXPECT_EQ(account_manager_->HasRestrictedIdentities(), false);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity1), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity2), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity3), true);
  EXPECT_EQ(account_manager_->IsValidIdentity(identity4), true);
  EXPECT_EQ(account_manager_->HasIdentities(), true);
  EXPECT_EQ((int)[account_manager_->GetAllIdentities() count], 4);
}
