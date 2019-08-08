// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/unified_consent/feature.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller_egtest_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SyncSettingsConfirmButton;

// Sign-in interaction tests that work both with Unified Consent enabled or
// disabled.
@interface SigninInteractionControllerTestCase : ChromeTestCase
@end

@implementation SigninInteractionControllerTestCase

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInOneUser {
  // Set up a fake identity.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [SigninEarlGreyUI signinWithIdentity:identity];

  // Check |identity| is signed-in.
  CHROME_EG_ASSERT_NO_ERROR(
      [SigninEarlGreyUtils checkSignedInWithIdentity:identity]);
}

// Tests that signing out from the Settings works correctly.
- (void)testSignInDisconnectFromChrome {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Sign in to |identity|.
  [SigninEarlGreyUI signinWithIdentity:identity];
  [ChromeEarlGreyUI openSettingsMenu];

  // Go to Accounts Settings and tap the sign out button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];

  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SignOutAccountsButton(),
                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Check that there is no signed in user.
  CHROME_EG_ASSERT_NO_ERROR([SigninEarlGreyUtils checkSignedOut]);
}

// Tests that signing out of a managed account from the Settings works
// correctly.
// TODO(crbug.com): Re-enable the test.
- (void)DISABLED_testSignInDisconnectFromChromeManaged {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeManagedIdentity];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];

  // Synchronization off due to an infinite spinner.
  SetEarlGreySynchronizationEnabled(NO);
  WaitForMatcher(chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON));
  TapButtonWithLabelId(IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON);
  SetEarlGreySynchronizationEnabled(YES);

  [SigninEarlGreyUI confirmSigninConfirmationDialog];
  CHROME_EG_ASSERT_NO_ERROR(
      [SigninEarlGreyUtils checkSignedInWithIdentity:identity]);

  // Go to Accounts Settings and tap the sign out button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];

  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SignOutAccountsButton(),
                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_MANAGED_DISCONNECT_DIALOG_ACCEPT);
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Check that there is no signed in user.
  CHROME_EG_ASSERT_NO_ERROR([SigninEarlGreyUtils checkSignedOut]);
}

// Tests that signing in, tapping the Settings link on the confirmation screen
// and closing the Settings correctly leaves the user signed in without any
// Settings shown.
- (void)testSignInOpenSettings {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];

  // Wait until the next screen appears.
  [SigninEarlGreyUI tapSettingsLink];

  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        performAction:grey_tap()];
  }

  // All Settings should be gone and user signed in.
  id<GREYMatcher> settings_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_SETTINGS_TITLE);
  [[EarlGrey selectElementWithMatcher:settings_matcher]
      assertWithMatcher:grey_notVisible()];
  CHROME_EG_ASSERT_NO_ERROR(
      [SigninEarlGreyUtils checkSignedInWithIdentity:identity]);
}

// Opens the sign in screen and then cancel it by opening a new tab. Ensures
// that the sign in screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelIdentityPicker {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Open new tab to cancel sign-in.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveBrowserViewController()
      openURLInNewTab:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  if (unified_consent::IsUnifiedConsentFeatureEnabled())
    [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];

  VerifyChromeSigninViewVisible();

  // Close sign-in screen and Settings.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Starts an authentication flow and cancel it by opening a new tab. Ensures
// that the authentication flow is correctly canceled and dismissed.
// crbug.com/462202
- (void)testSignInCancelAuthenticationFlow {
  // Set up the fake identities.
  ios::FakeChromeIdentityService* identity_service =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  ChromeIdentity* identity1 = [SigninEarlGreyUtils fakeIdentity1];
  ChromeIdentity* identity2 = [SigninEarlGreyUtils fakeIdentity2];
  identity_service->AddIdentity(identity1);
  identity_service->AddIdentity(identity2);

  // This signs in |identity2| first, ensuring that the "Clear Data Before
  // Syncing" dialog is shown during the second sign-in. This dialog will
  // effectively block the authentication flow, ensuring that the authentication
  // flow is always still running when the sign-in is being cancelled.
  [SigninEarlGreyUI signinWithIdentity:identity2];
  [ChromeEarlGreyUI openSettingsMenu];

  // Go to Accounts Settings and tap the sign out button.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsAccountButton()]
      performAction:grey_tap()];

  const CGFloat scroll_displacement = 100.0;
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   chrome_test_util::SignOutAccountsButton(),
                                   grey_interactable(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  scroll_displacement)
      onElementWithMatcher:chrome_test_util::SettingsAccountsCollectionView()]
      performAction:grey_tap()];
  TapButtonWithLabelId(IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE);
  CHROME_EG_ASSERT_NO_ERROR([SigninEarlGreyUtils checkSignedOut]);

  // Sign in with |identity1|.
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI selectIdentityWithEmail:identity1.userEmail];
  if (unified_consent::IsUnifiedConsentFeatureEnabled()) {
    // With unified consent, the authentication flow is only created when the
    // confirm button is selected. Note that authentication flow actually
    /// blocks as the "Clear Browsing Before Syncing" dialog is presented.
    [SigninEarlGreyUI confirmSigninConfirmationDialog];
  }

  // Open new tab to cancel sign-in.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveBrowserViewController()
      openURLInNewTab:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  if (unified_consent::IsUnifiedConsentFeatureEnabled())
    [SigninEarlGreyUI selectIdentityWithEmail:identity1.userEmail];
  VerifyChromeSigninViewVisible();

  // Close sign-in screen and Settings.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  CHROME_EG_ASSERT_NO_ERROR([SigninEarlGreyUtils checkSignedOut]);
}

// Opens the sign in screen from the bookmarks and then cancel it by tapping on
// done. Ensures that the sign in screen is correctly dismissed.
// Regression test for crbug.com/596029.
- (void)testSignInCancelFromBookmarks {
  ChromeIdentity* identity = [SigninEarlGreyUtils fakeIdentity1];
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      identity);

  // Open Bookmarks and tap on Sign In promo button.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Assert sign-in screen was shown.
  if (unified_consent::IsUnifiedConsentFeatureEnabled())
    [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];
  VerifyChromeSigninViewVisible();

  // Open new tab to cancel sign-in.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL("about:blank")];
  [chrome_test_util::DispatcherForActiveBrowserViewController()
      openURLInNewTab:command];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  if (unified_consent::IsUnifiedConsentFeatureEnabled())
    [SigninEarlGreyUI selectIdentityWithEmail:identity.userEmail];
  VerifyChromeSigninViewVisible();

  // Close sign-in screen and Bookmarks.
  TapButtonWithLabelId(IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON);
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarDoneButton()]
      performAction:grey_tap()];
}

@end
