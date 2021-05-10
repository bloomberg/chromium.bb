// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#import "components/signin/public/base/account_consistency_method.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GoogleServicesSettingsView;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SyncSettingsConfirmButton;
using l10n_util::GetNSString;
using testing::ButtonWithAccessibilityLabel;

namespace {

// Timeout in seconds to wait for asynchronous sync operations.
const NSTimeInterval kSyncOperationTimeout = 5.0;

}  // namespace

// Interaction tests for advanced settings sign-in with
// |kMobileIdentityConsistency| feature enabled.
@interface AdvancedSettingsSigninTestCase : ChromeTestCase
@end

@implementation AdvancedSettingsSigninTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(signin::kMobileIdentityConsistency);
  return config;
}

// Tests that signing in, tapping the Settings link on the confirmation screen
// and closing the advanced sign-in settings correctly leaves the user signed
// in.
- (void)testSignInOpenSyncSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];
  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  // Test the user is signed in.
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Verifies that advanced sign-in shows an alert dialog when being swiped to
// dismiss.
- (void)testSwipeDownToCancelAdvancedSignin {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [SigninEarlGreyUI tapSettingsLink];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kManageSyncTableViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [[EarlGrey
      selectElementWithMatcher:
          ButtonWithAccessibilityLabel(GetNSString(
              IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_CANCEL_SYNC_BUTTON))]
      performAction:grey_tap()];

  [SigninEarlGrey verifySignedOut];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: advanced sign-in.
- (void)testInterruptAdvancedSigninSettingsFromAdvancedSigninSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [ChromeEarlGreyUI waitForAppToIdle];

  [SigninEarlGreyUI tapSettingsLink];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey simulateExternalAppURLOpening];

  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: advanced sign-in.
- (void)testInterruptAdvancedSigninBookmarksFromAdvancedSigninSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI tapSettingsLink];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey simulateExternalAppURLOpening];

  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: advanced sign-in.
- (void)testInterruptSigninFromRecentTabsFromAdvancedSigninSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI tapPrimarySignInButtonInRecentTabs];
  [SigninEarlGreyUI tapSettingsLink];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey simulateExternalAppURLOpening];

  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

// Tests interrupting sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: advanced sign-in.
- (void)testInterruptSigninFromTabSwitcherFromAdvancedSigninSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGrey fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI tapPrimarySignInButtonInTabSwitcher];
  [SigninEarlGreyUI tapSettingsLink];
  [ChromeEarlGreyUI waitForAppToIdle];

  [ChromeEarlGrey simulateExternalAppURLOpening];

  [ChromeEarlGrey waitForSyncInitialized:NO syncTimeout:kSyncOperationTimeout];
  [SigninEarlGrey verifySignedInWithFakeIdentity:fakeIdentity];
}

@end
