// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/policy/core/common/policy_loader_ios_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_matchers.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/common/string_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager_constants.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#include "ios/third_party/earl_grey2/src/CommonLib/Matcher/GREYLayoutConstraint.h"  // nogncheck

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::IdentityCellMatcherForEmail;
using chrome_test_util::SyncSettingsConfirmButton;

namespace {

NSString* const kScrollViewIdentifier =
    @"kFirstRunScrollViewAccessibilityIdentifier";

// Returns a matcher for the welcome screen accept button.
id<GREYMatcher> GetAcceptButton() {
  return grey_allOf(grey_text(l10n_util::GetNSString(
                        IDS_IOS_FIRST_RUN_WELCOME_SCREEN_ACCEPT_BUTTON)),
                    grey_sufficientlyVisible(), nil);
}

// Returns a matcher for the sign-in screen "Continue as <identity>" button.
id<GREYMatcher> GetContinueButtonWithIdentity(
    FakeChromeIdentity* fakeIdentity) {
  NSString* buttonTitle = l10n_util::GetNSStringF(
      IDS_IOS_FIRST_RUN_SIGNIN_CONTINUE_AS,
      base::SysNSStringToUTF16(fakeIdentity.userGivenName));
  return grey_accessibilityLabel(buttonTitle);
}

// Returns a constraint where the element is below the reference.
GREYLayoutConstraint* BelowConstraint() {
  return [GREYLayoutConstraint
      layoutConstraintWithAttribute:kGREYLayoutAttributeTop
                          relatedBy:kGREYLayoutRelationGreaterThanOrEqual
               toReferenceAttribute:kGREYLayoutAttributeBottom
                         multiplier:1.0
                           constant:0.0];
}

}  // namespace

// Test first run stages
@interface FirstRunTestCase : ChromeTestCase

@end

@implementation FirstRunTestCase

- (void)setUp {
  [[self class] testForStartup];

  [super setUp];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (void)tearDown {
  [super tearDown];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_disabled.push_back(kLocationPermissionsPrompt);

  config.features_enabled.push_back(kEnableFREUIModuleIOS);

  // Show the First Run UI at startup.
  config.additional_args.push_back("-FirstRunForceEnabled");
  config.additional_args.push_back("true");

  // Relaunch app at each test to rewind the startup state.
  config.relaunch_policy = ForceRelaunchByKilling;

  return config;
}

#pragma mark - Helpers

// Checks that the welcome screen is displayed.
- (void)verifyWelcomeScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunWelcomeScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the sign in screen is displayed.
- (void)verifySignInScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the sync screen is displayed.
- (void)verifySyncScreenIsDisplayed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that the default browser screen is displayed.
- (void)verifyDefaultBrowserScreenIsDisplayed {
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_notNil()];
}

// Checks that none of any FRE's screen is displayed.
- (void)verifyFREIsDismissed {
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunWelcomeScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSignInScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     first_run::kFirstRunSyncScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(
              first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

// Scrolls down to |elementMatcher| in the scrollable content of the first run
// screen.
- (void)scrollToElementAndAssertVisibility:(id<GREYMatcher>)elementMatcher {
  id<GREYMatcher> scrollView = grey_accessibilityID(kScrollViewIdentifier);

  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(elementMatcher,
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollView] assertWithMatcher:grey_notNil()];
}

#pragma mark - Tests

// Checks that the Welcome screen is displayed correctly.
- (void)testWelcomeScreenUI {
  [self verifyWelcomeScreenIsDisplayed];

  // Validate the Title text.
  NSString* expectedTitleText =
      [ChromeEarlGrey isIPadIdiom]
          ? l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPAD)
          : l10n_util::GetNSString(
                IDS_IOS_FIRST_RUN_WELCOME_SCREEN_TITLE_IPHONE);
  id<GREYMatcher> title = grey_text(expectedTitleText);
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_SUBTITLE));
  [self scrollToElementAndAssertVisibility:subtitle];

  // Validate the Metrics Consent box.
  id<GREYMatcher> metricsConsent = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_WELCOME_SCREEN_METRICS_CONSENT));
  [self scrollToElementAndAssertVisibility:metricsConsent];

  // Validate the Accept box.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
}

// Checks that the Sign In screen is displayed correctly.
- (void)testSignInScreenUI {
  [self verifyWelcomeScreenIsDisplayed];

  // Go to the sign-in screen.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];

  // Validate the Title text.
  id<GREYMatcher> title =
      grey_text(l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_TITLE));
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle =
      grey_text(l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SIGNIN_SUBTITLE));
  [self scrollToElementAndAssertVisibility:subtitle];
}

// Checks that the default browser screen is displayed correctly.
- (void)testDefaultBrowserScreenUI {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;
  config.features_enabled.push_back(kEnableFREDefaultBrowserScreen);
  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Go to the default browser screen.
  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];
  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN))]
      performAction:grey_tap()];

  [self verifyDefaultBrowserScreenIsDisplayed];

  // Validate the Title text.
  id<GREYMatcher> title = grey_text(
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_TITLE));
  [self scrollToElementAndAssertVisibility:title];

  // Validate the Subtitle text.
  id<GREYMatcher> subtitle = grey_text(l10n_util::GetNSString(
      IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SUBTITLE));
  [self scrollToElementAndAssertVisibility:subtitle];

  // Remove bold tags in instructions.
  StringWithTag firstInstructionParsed = ParseStringWithTag(
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_FIRST_STEP),
      first_run::kBeginBoldTag, first_run::kEndBoldTag);
  StringWithTag secondInstructionParsed = ParseStringWithTag(
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_SECOND_STEP),
      first_run::kBeginBoldTag, first_run::kEndBoldTag);
  StringWithTag thirdInstructionParsed = ParseStringWithTag(
      l10n_util::GetNSString(
          IDS_IOS_FIRST_RUN_DEFAULT_BROWSER_SCREEN_THIRD_STEP),
      first_run::kBeginBoldTag, first_run::kEndBoldTag);

  // Verify instruction order.
  id<GREYMatcher> firstInstruction = grey_text(firstInstructionParsed.string);
  id<GREYMatcher> secondInstruction = grey_text(secondInstructionParsed.string);
  id<GREYMatcher> thirdInstruction = grey_text(thirdInstructionParsed.string);

  // Scroll to ensure that the third instruction is visible.
  id<GREYMatcher> scrollViewMatcher =
      grey_accessibilityID(kScrollViewIdentifier);
  [[EarlGrey selectElementWithMatcher:thirdInstruction]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:scrollViewMatcher];

  [[EarlGrey selectElementWithMatcher:secondInstruction]
      assertWithMatcher:grey_layout(@[ BelowConstraint() ], firstInstruction)];
  [[EarlGrey selectElementWithMatcher:thirdInstruction]
      assertWithMatcher:grey_layout(@[ BelowConstraint() ], secondInstruction)];
}

// Navigates to the Terms of Service and back.
- (void)testTermsAndConditions {
  // Tap on “Terms of Service” on the first screen
  [self verifyWelcomeScreenIsDisplayed];

  // Scroll to and open the ToS screen.
  id<GREYMatcher> termsOfServiceLink =
      grey_accessibilityLabel(@"Terms of Service");
  [self scrollToElementAndAssertVisibility:termsOfServiceLink];
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_TERMS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on “Done” on the ToS screen
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [self verifyWelcomeScreenIsDisplayed];

  // Scroll to and tap the accept ToS button.
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];
}

// Tests that the FRE is shown when incognito is forced by policy.
- (void)testFirstRunWithIncognitoForced {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;

  std::string policy_data = "<dict>"
                            "    <key>IncognitoModeAvailability</key>"
                            "    <integer>2</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  // Verify that the FRE UI is shown while the browser is in incognito mode.
  [self verifyWelcomeScreenIsDisplayed];
}

// Tests that the FRE sign in screen is not displayed when sign in is disabled
// by policy.
- (void)testSignInDisable {
  AppLaunchConfiguration config = self.appConfigurationForTestCase;

  // Configure the policy to disable SignIn.
  std::string policy_data = "<dict>"
                            "    <key>BrowserSignin</key>"
                            "    <integer>0</integer>"
                            "</dict>";
  base::RemoveChars(policy_data, base::kWhitespaceASCII, &policy_data);

  config.additional_args.push_back("-EnableSamplePolicies");
  config.additional_args.push_back(
      "-" + base::SysNSStringToUTF8(kPolicyLoaderIOSConfigurationKey));
  config.additional_args.push_back(policy_data);

  // Relaunch the app to take the configuration into account.
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];
  [self verifyFREIsDismissed];
}

// Checks that when opening the app no accounts are here and the primary button
// allows to create a new account and that it is updated if a new account is
// added.
- (void)testSignInNoAccount {
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION))]
      performAction:grey_tap()];

  // Check for the fake SSO screen.
  [ChromeEarlGrey
      waitForMatcher:grey_accessibilityID(kFakeAddAccountViewIdentifier)];
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(@"Cancel"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [ChromeEarlGreyUI waitForAppToIdle];

  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  // Check that the title of the primary button updates for |fakeIdentity|.
  [[EarlGrey
      selectElementWithMatcher:GetContinueButtonWithIdentity(fakeIdentity)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_SIGN_IN_ACTION))]
      assertWithMatcher:grey_nil()];
}

// Checks that it is possible to add an account even if there is already account
// and that it is possible to switch accounts when multiple accounts are
// present.
- (void)testSignInSelectAccount {
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGrey fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGrey fakeIdentity2];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity2];

  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];

  // Check that |fakeIdentity2| is displayed.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Check that 'Add Account' is displayed.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Select |fakeIdentity2|.
  [[EarlGrey selectElementWithMatcher:IdentityCellMatcherForEmail(
                                          fakeIdentity2.userEmail)]
      performAction:grey_tap()];

  // Check that the title of the primary button updates for |fakeIdentity2|.
  [[EarlGrey
      selectElementWithMatcher:GetContinueButtonWithIdentity(fakeIdentity2)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks that pressing "No thanks" on SignIn screen doesn't sign in the user.
- (void)testNoSignIn {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];

  [[EarlGrey
      selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                   IDS_IOS_FIRST_RUN_SIGNIN_DONT_SIGN_IN))]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];
}

// Checks that sync is turned on after the user chose to turn on sync.
- (void)testTurnOnSync {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];
  [[EarlGrey
      selectElementWithMatcher:GetContinueButtonWithIdentity(fakeIdentity)]
      performAction:grey_tap()];

  [self verifySyncScreenIsDisplayed];
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_text(l10n_util::GetNSString(
                                IDS_IOS_FIRST_RUN_SYNC_SCREEN_PRIMARY_ACTION)),
                            grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Checks that sync is not turned on if an account has been signed in but the
// user chose not to turn on sync.
- (void)testNoSync {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];
  [[EarlGrey
      selectElementWithMatcher:GetContinueButtonWithIdentity(fakeIdentity)]
      performAction:grey_tap()];

  [self verifySyncScreenIsDisplayed];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_text(l10n_util::GetNSString(
                         IDS_IOS_FIRST_RUN_SYNC_SCREEN_SECONDARY_ACTION)),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  // Verify that the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:NO];
}

// Checks that the FRE is dismissed when the user taps on the advanced sync
// settings button and that sync is turned on after the user chose to turn on
// sync in the advanced sync settings screen.
- (void)testCustomSyncOn {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];
  [[EarlGrey
      selectElementWithMatcher:GetContinueButtonWithIdentity(fakeIdentity)]
      performAction:grey_tap()];

  [self verifySyncScreenIsDisplayed];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_text(l10n_util::GetNSString(
                         IDS_IOS_FIRST_RUN_SYNC_SCREEN_ADVANCE_SETTINGS)),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [self verifyFREIsDismissed];

  // Check that Sync hasn't started yet, allowing the user to change some
  // settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  GREYAssertTrue([FirstRunAppInterface isSyncFirstSetupComplete],
                 @"Sync should have finished its original setup");

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:YES];
}

// Checks that the user is signed in and sync is turned off after the user chose
// to cancel sync in the advanced sync setting screen.
- (void)testCustomSyncOff {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [self verifyWelcomeScreenIsDisplayed];
  [self scrollToElementAndAssertVisibility:GetAcceptButton()];
  [[EarlGrey selectElementWithMatcher:GetAcceptButton()]
      performAction:grey_tap()];

  [self verifySignInScreenIsDisplayed];
  [[EarlGrey
      selectElementWithMatcher:GetContinueButtonWithIdentity(fakeIdentity)]
      performAction:grey_tap()];

  [self verifySyncScreenIsDisplayed];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_text(l10n_util::GetNSString(
                         IDS_IOS_FIRST_RUN_SYNC_SCREEN_ADVANCE_SETTINGS)),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];

  [self verifyFREIsDismissed];

  // Check that Sync hasn't started yet, allowing the user to change some
  // settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  // Cancel sync.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarCancelButton()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              grey_text(l10n_util::GetNSString(
                  IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_CANCEL_SYNC_BUTTON)),
              grey_sufficientlyVisible(), nil)] performAction:grey_tap()];

  // Check that the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [SigninEarlGrey verifySyncUIEnabled:NO];
}

@end
