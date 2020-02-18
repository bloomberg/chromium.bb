// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/first_run/first_run_app_interface.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::SyncSettingsConfirmButton;

namespace {

// Returns matcher for the opt in accept button.
id<GREYMatcher> FirstRunOptInAcceptButton() {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSString(IDS_IOS_FIRSTRUN_OPT_IN_ACCEPT_BUTTON));
}

// Returns matcher for the skip sign in button.
id<GREYMatcher> SkipSigninButton() {
  return grey_accessibilityID(
      first_run::kSignInSkipButtonAccessibilityIdentifier);
}
}

// Tests first run settings and navigation.
@interface FirstRunTestCase : ChromeTestCase
@end

@implementation FirstRunTestCase

- (void)setUp {
  [super setUp];
  [FirstRunAppInterface setUMACollectionEnabled:NO];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

- (void)tearDown {
  [super tearDown];
  [FirstRunAppInterface resetUMACollectionEnabledByDefault];
}

// Navigates to the terms of service and back.
- (void)testPrivacy {
  [FirstRunAppInterface showFirstRunUI];

  id<GREYMatcher> privacyLink = grey_accessibilityLabel(@"Privacy Notice");
  [[EarlGrey selectElementWithMatcher:privacyLink] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_PRIVACY_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [[EarlGrey selectElementWithMatcher:privacyLink]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Navigates to the terms of service and back.
- (void)testTermsAndConditions {
  [FirstRunAppInterface showFirstRunUI];

  id<GREYMatcher> termsOfServiceLink =
      grey_accessibilityLabel(@"Terms of Service");
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:grey_text(l10n_util::GetNSString(
                                          IDS_IOS_FIRSTRUN_TERMS_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(@"ic_arrow_back"),
                                   grey_accessibilityTrait(
                                       UIAccessibilityTraitButton),
                                   nil)] performAction:grey_tap()];

  // Ensure we went back to the First Run screen.
  [[EarlGrey selectElementWithMatcher:termsOfServiceLink]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Toggle the UMA checkbox.
- (void)testToggleMetricsOn {
  [FirstRunAppInterface showFirstRunUI];

  id<GREYMatcher> metrics =
      grey_accessibilityID(first_run::kUMAMetricsButtonAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:metrics] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  GREYAssert([FirstRunAppInterface isUMACollectionEnabled] !=
                 [FirstRunAppInterface isUMACollectionEnabledByDefault],
             @"Metrics reporting pref is incorrect.");
}

// Dismisses the first run screens.
- (void)testDismissFirstRun {
  [FirstRunAppInterface showFirstRunUI];

  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  GREYAssert([FirstRunAppInterface isUMACollectionEnabled] ==
                 [FirstRunAppInterface isUMACollectionEnabledByDefault],
             @"Metrics reporting does not match.");

  [[EarlGrey selectElementWithMatcher:SkipSigninButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Signs in to an account and then taps the Advanced link to go to settings.
- (void)testSignInAndTapSettingsLink {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  // Launch First Run and accept tems of services.
  [FirstRunAppInterface showFirstRunUI];
  [[EarlGrey selectElementWithMatcher:FirstRunOptInAcceptButton()]
      performAction:grey_tap()];

  // Tap Settings link.
  [SigninEarlGreyUI tapSettingsLink];

  // Check Sync hasn't started yet, allowing the user to change some settings.
  GREYAssertFalse([FirstRunAppInterface isSyncFirstSetupComplete],
                  @"Sync shouldn't have finished its original setup yet");

  // Close Settings, user is still signed in and sync is now starting.
  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity];

  GREYAssertTrue([FirstRunAppInterface isSyncFirstSetupComplete],
                 @"Sync should have finished its original setup");
}

@end
