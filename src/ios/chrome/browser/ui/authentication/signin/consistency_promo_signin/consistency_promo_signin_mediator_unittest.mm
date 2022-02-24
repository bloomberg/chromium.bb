// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_mediator.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Subclass of ConsistencyPromoSigninMediator to override
// |signinTimeoutDurationSeconds| property.
@interface TestConsistencyPromoSigninMediator : ConsistencyPromoSigninMediator

@property(nonatomic, assign) NSInteger signinTimeoutDurationSeconds;

@end

@implementation TestConsistencyPromoSigninMediator
@end

class ConsistencyPromoSigninMediatorTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    identity1_ = [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                                gaiaID:@"foo1ID1"
                                                  name:@"Fake Foo 1"];
    identity2_ = [FakeChromeIdentity identityWithEmail:@"foo2@gmail.com"
                                                gaiaID:@"foo1ID2"
                                                  name:@"Fake Foo 2"];
    GetIdentityService()->AddIdentity(identity1_);
    GetIdentityService()->AddIdentity(identity2_);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    browser_state_ = builder.Build();

    PrefRegistrySimple* registry = pref_service_.registry();
    registry->RegisterIntegerPref(prefs::kSigninWebSignDismissalCount, 0);

    mediator_delegate_mock_ = OCMStrictProtocolMock(
        @protocol(ConsistencyPromoSigninMediatorDelegate));
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY((id)mediator_delegate_mock_);
    PlatformTest::TearDown();
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForBrowserState(browser_state_.get());
  }

  ios::FakeChromeIdentityService* GetIdentityService() {
    return ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  }

  AuthenticationService* GetAuthenticationService() {
    return AuthenticationServiceFactory::GetForBrowserState(
        browser_state_.get());
  }

  TestConsistencyPromoSigninMediator* GetConsistencyPromoSigninMediator() {
    ChromeAccountManagerService* chromeAccountManagerService =
        ChromeAccountManagerServiceFactory::GetForBrowserState(
            browser_state_.get());
    TestConsistencyPromoSigninMediator* mediator =
        [[TestConsistencyPromoSigninMediator alloc]
            initWithAccountManagerService:chromeAccountManagerService
                    authenticationService:GetAuthenticationService()
                          identityManager:GetIdentityManager()
                          userPrefService:&pref_service_];
    mediator.delegate = mediator_delegate_mock_;
    return mediator;
  }

  // Signs in and simulates cookies being added on the web.
  void SigninAndSimulateCookies(ConsistencyPromoSigninMediator* mediator,
                                ChromeIdentity* identity) {
    GetAuthenticationService()->SignIn(identity, nil);
    OCMExpect([mediator_delegate_mock_
        consistencyPromoSigninMediatorSignInDone:mediator
                                    withIdentity:identity]);
    id<IdentityManagerObserverBridgeDelegate>
        identityManagerObserverBridgeDelegate =
            (id<IdentityManagerObserverBridgeDelegate>)mediator;
    struct signin::AccountsInCookieJarInfo cookieJarInfo;
    gaia::ListedAccount account;
    account.id = CoreAccountId(base::SysNSStringToUTF8(identity.gaiaID));
    cookieJarInfo.signed_in_accounts.push_back(account);
    const GoogleServiceAuthError error(GoogleServiceAuthError::State::NONE);
    [identityManagerObserverBridgeDelegate
        onAccountsInCookieUpdated:cookieJarInfo
                            error:error];
  }

  // Signs in and simulates a cookie error.
  void SigninAndSimulateError(ConsistencyPromoSigninMediator* mediator,
                              ChromeIdentity* identity) {
    GetAuthenticationService()->SignIn(identity, nil);
    OCMExpect([mediator_delegate_mock_
        consistencyPromoSigninMediator:mediator
                        errorDidHappen:
                            ConsistencyPromoSigninMediatorErrorGeneric]);
    id<IdentityManagerObserverBridgeDelegate>
        identityManagerObserverBridgeDelegate =
            (id<IdentityManagerObserverBridgeDelegate>)mediator;
    struct signin::AccountsInCookieJarInfo cookieJarInfo;
    gaia::ListedAccount account;
    account.id = CoreAccountId(base::SysNSStringToUTF8(identity.gaiaID));
    cookieJarInfo.signed_in_accounts.push_back(account);
    const GoogleServiceAuthError error(
        GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
    [identityManagerObserverBridgeDelegate
        onAccountsInCookieUpdated:cookieJarInfo
                            error:error];
  }

 protected:
  // Needed for test browser state created by TestChromeBrowserState().
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;

  FakeChromeIdentity* identity1_ = nullptr;
  FakeChromeIdentity* identity2_ = nullptr;

  id<ConsistencyPromoSigninMediatorDelegate> mediator_delegate_mock_ = nil;
};

// Tests start and cancel by user.
TEST_F(ConsistencyPromoSigninMediatorTest, StartAndStopForCancel) {
  base::HistogramTester histogram_tester;
  ConsistencyPromoSigninMediator* mediator =
      GetConsistencyPromoSigninMediator();
  [mediator disconnectWithResult:SigninCoordinatorResultCanceledByUser];

  histogram_tester.ExpectTotalCount("Signin.AccountConsistencyPromoAction", 2);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::DISMISSED_BUTTON, 1);
}

// Tests start and interrupt.
TEST_F(ConsistencyPromoSigninMediatorTest, StartAndStopForInterrupt) {
  base::HistogramTester histogram_tester;
  ConsistencyPromoSigninMediator* mediator =
      GetConsistencyPromoSigninMediator();
  [mediator disconnectWithResult:SigninCoordinatorResultInterrupted];

  histogram_tester.ExpectTotalCount("Signin.AccountConsistencyPromoAction", 2);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::DISMISSED_OTHER, 1);
}

// Tests start and sign-in with default identity.
TEST_F(ConsistencyPromoSigninMediatorTest,
       SigninCoordinatorResultSuccessWithDefaultIdentity) {
  base::HistogramTester histogram_tester;
  pref_service_.SetInteger(prefs::kSigninWebSignDismissalCount, 1);
  ConsistencyPromoSigninMediator* mediator =
      GetConsistencyPromoSigninMediator();
  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSigninStarted:mediator]);
  [mediator signinWithIdentity:identity1_];
  SigninAndSimulateCookies(mediator, identity1_);
  EXPECT_EQ(0, pref_service_.GetInteger(prefs::kSigninWebSignDismissalCount));
  [mediator disconnectWithResult:SigninCoordinatorResultSuccess];

  histogram_tester.ExpectTotalCount("Signin.AccountConsistencyPromoAction", 2);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::
          SIGNED_IN_WITH_DEFAULT_ACCOUNT,
      1);
}

// Tests start and sign-in with secondary identity.
TEST_F(ConsistencyPromoSigninMediatorTest,
       SigninCoordinatorResultSuccessWithSecondaryIdentity) {
  base::HistogramTester histogram_tester;
  ConsistencyPromoSigninMediator* mediator =
      GetConsistencyPromoSigninMediator();
  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSigninStarted:mediator]);
  [mediator signinWithIdentity:identity2_];
  SigninAndSimulateCookies(mediator, identity2_);
  [mediator disconnectWithResult:SigninCoordinatorResultSuccess];

  histogram_tester.ExpectTotalCount("Signin.AccountConsistencyPromoAction", 2);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::
          SIGNED_IN_WITH_NON_DEFAULT_ACCOUNT,
      1);
}

// Tests start and sign-in with an added identity.
TEST_F(ConsistencyPromoSigninMediatorTest,
       SigninCoordinatorResultSuccessWithAddedIdentity) {
  base::HistogramTester histogram_tester;
  FakeChromeIdentity* identity3 =
      [FakeChromeIdentity identityWithEmail:@"foo3@gmail.com"
                                     gaiaID:@"foo1ID3"
                                       name:@"Fake Foo 3"];
  GetIdentityService()->AddIdentity(identity3);
  ConsistencyPromoSigninMediator* mediator =
      GetConsistencyPromoSigninMediator();
  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSigninStarted:mediator]);
  [mediator signinWithIdentity:identity3];
  [mediator chromeIdentityAdded:identity3];
  SigninAndSimulateCookies(mediator, identity3);
  [mediator disconnectWithResult:SigninCoordinatorResultSuccess];

  histogram_tester.ExpectTotalCount("Signin.AccountConsistencyPromoAction", 2);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::
          SIGNED_IN_WITH_ADDED_ACCOUNT,
      1);
}

// Tests start and sign-in with an error.
TEST_F(ConsistencyPromoSigninMediatorTest, SigninCoordinatorWithError) {
  base::HistogramTester histogram_tester;
  ConsistencyPromoSigninMediator* mediator =
      GetConsistencyPromoSigninMediator();
  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSigninStarted:mediator]);
  [mediator signinWithIdentity:identity1_];
  SigninAndSimulateError(mediator, identity1_);
  [mediator disconnectWithResult:SigninCoordinatorResultCanceledByUser];

  histogram_tester.ExpectTotalCount("Signin.AccountConsistencyPromoAction", 3);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::DISMISSED_BUTTON, 1);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::GENERIC_ERROR_SHOWN, 1);
}

// Tests timeout error.
TEST_F(ConsistencyPromoSigninMediatorTest, SigninCoordinatorWithTimeoutError) {
  base::HistogramTester histogram_tester;
  TestConsistencyPromoSigninMediator* mediator =
      GetConsistencyPromoSigninMediator();
  // Sets the timeout duration to 0, to trigger the timeout error without
  // waiting.
  mediator.signinTimeoutDurationSeconds = 0;
  // Starts sign-in for the mediator.
  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediatorSigninStarted:mediator]);
  [mediator signinWithIdentity:identity1_];
  // Expects timeout.
  OCMExpect([mediator_delegate_mock_
      consistencyPromoSigninMediator:mediator
                      errorDidHappen:
                          ConsistencyPromoSigninMediatorErrorTimeout]);
  // Wait for the time trigger.
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount("Signin.AccountConsistencyPromoAction", 2);
  // Expects show metric.
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::SHOWN, 1);
  // Expects timeout metric.
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::TIMEOUT_ERROR_SHOWN, 1);
  // Closes the sign-in dialog.
  [mediator disconnectWithResult:SigninCoordinatorResultCanceledByUser];
  // Expects dismiss metric.
  histogram_tester.ExpectTotalCount("Signin.AccountConsistencyPromoAction", 3);
  histogram_tester.ExpectBucketCount(
      "Signin.AccountConsistencyPromoAction",
      signin_metrics::AccountConsistencyPromoAction::DISMISSED_BUTTON, 1);
}
