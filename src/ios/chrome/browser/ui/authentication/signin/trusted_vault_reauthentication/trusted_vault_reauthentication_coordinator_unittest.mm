// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

#import "base/test/ios/wait_util.h"
#import "components/sync/driver/sync_service_utils.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_trusted_vault_service.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class TrustedVaultReauthenticationCoordinatorTest : public PlatformTest {
 public:
  TrustedVaultReauthenticationCoordinatorTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    base_view_controller_ = [[UIViewController alloc] init];
    base_view_controller_.view.backgroundColor = UIColor.blueColor;
    GetAnyKeyWindow().rootViewController = base_view_controller_;

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = builder.Build();

    ChromeIdentity* identity =
        [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                       gaiaID:@"foo1ID"
                                         name:@"Fake Foo 1"];
    ios::FakeChromeIdentityService* identity_service =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service->AddIdentity(identity);
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state_.get());
    authentication_service->SignIn(identity);

    WebStateList* web_state_list = nullptr;
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), web_state_list);
  }

  Browser* browser() { return browser_.get(); }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;

  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;

  UIViewController* base_view_controller_ = nil;
};

// Opens the trusted vault reauth dialog, and simulate a user cancel.
TEST_F(TrustedVaultReauthenticationCoordinatorTest, TestCancel) {
  // Create sign-in coordinator.
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  SigninCoordinator* signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          base_view_controller_
                                                            browser:browser()
                                                             intent:
                                         SigninTrustedVaultDialogIntentFetchKeys
                                                            trigger:trigger];
  // Open and cancel the web sign-in dialog.
  __block bool signin_completion_called = false;
  signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        signin_completion_called = true;
        EXPECT_EQ(SigninCoordinatorResultCanceledByUser, result);
        EXPECT_EQ(nil, info.identity);
      };
  [signinCoordinator start];
  // Wait until the view controllre is presented.
  EXPECT_NE(nil, base_view_controller_.presentedViewController);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return !base_view_controller_.presentedViewController.beingPresented;
      }));
  ios::FakeChromeTrustedVaultService::GetInstanceFromChromeProvider()
      ->SimulateUserCancel();
  // Test the completion block.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return signin_completion_called;
      }));
}

// Opens the trusted vault reauth dialog, and simulate a user cancel.
TEST_F(TrustedVaultReauthenticationCoordinatorTest, TestInterrupt) {
  // Create sign-in coordinator.
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  SigninCoordinator* signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          base_view_controller_
                                                            browser:browser()
                                                             intent:
                                         SigninTrustedVaultDialogIntentFetchKeys
                                                            trigger:trigger];
  // Open and cancel the web sign-in dialog.
  __block bool signin_completion_called = false;
  signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* info) {
        signin_completion_called = true;
        EXPECT_EQ(SigninCoordinatorResultInterrupted, result);
        EXPECT_EQ(nil, info.identity);
      };
  [signinCoordinator start];
  // Wait until the view controllre is presented.
  EXPECT_NE(nil, base_view_controller_.presentedViewController);
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return !base_view_controller_.presentedViewController.beingPresented;
      }));
  // Interrupt the coordinator.
  __block bool interrupt_completion_called = false;
  [signinCoordinator interruptWithAction:
                         SigninCoordinatorInterruptActionDismissWithoutAnimation
                              completion:^() {
                                EXPECT_TRUE(signin_completion_called);
                                interrupt_completion_called = true;
                              }];
  // Test the completion block.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return interrupt_completion_called;
      }));
}
