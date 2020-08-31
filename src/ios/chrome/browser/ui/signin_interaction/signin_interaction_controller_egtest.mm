// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"
#import "ios/chrome/browser/ui/authentication/signin_earlgrey_utils.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller_app_interface.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller_egtest_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager_constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/testing/earl_grey/matchers.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(SigninInteractionControllerAppInterface);
#pragma clang diagnostic pop
#endif  // defined(CHROME_EARL_GREY_2)

using chrome_test_util::BookmarksNavigationBarDoneButton;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::GoogleServicesSettingsView;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsImportDataContinueButton;
using chrome_test_util::SettingsImportDataImportButton;
using chrome_test_util::SettingsImportDataKeepSeparateButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::StaticTextWithAccessibilityLabelId;
using chrome_test_util::SyncSettingsConfirmButton;
using l10n_util::GetNSString;
using testing::ButtonWithAccessibilityLabel;

typedef NS_ENUM(NSInteger, OpenSigninMethod) {
  OpenSigninMethodFromSettings,
  OpenSigninMethodFromBookmarks,
  OpenSigninMethodFromRecentTabs,
  OpenSigninMethodFromTabSwitcher,
};

namespace {

// Taps on the primary sign-in button in recent tabs, and scroll first, if
// necessary.
void TapOnPrimarySignInButtonInRecentTabs() {
  id<GREYMatcher> matcher =
      grey_allOf(PrimarySignInButton(), grey_sufficientlyVisible(), nil);
  const CGFloat kPixelsToScroll = 300;
  id<GREYAction> searchAction =
      grey_scrollInDirection(kGREYDirectionDown, kPixelsToScroll);
  GREYElementInteraction* interaction =
      [[EarlGrey selectElementWithMatcher:matcher]
             usingSearchAction:searchAction
          onElementWithMatcher:
              grey_accessibilityID(
                  kRecentTabsTableViewControllerAccessibilityIdentifier)];
  [interaction performAction:grey_tap()];
}

// Returns a matcher for |userEmail| in IdentityChooserViewController.
id<GREYMatcher> identityChooserButtonMatcherWithEmail(NSString* userEmail) {
  return grey_allOf(grey_accessibilityID(userEmail),
                    grey_kindOfClassName(@"IdentityChooserCell"),
                    grey_sufficientlyVisible(), nil);
}

void ChooseImportOrKeepDataSepareteDialog(id<GREYMatcher> choiceButtonMatcher) {
  // The ChromeSigninView's activity indicator must be hidden as the import
  // data UI is presented on top of the activity indicator and Earl Grey cannot
  // interact with any UI while an animation is active.
  [SigninInteractionControllerAppInterface setActivityIndicatorShown:NO];

  // Set up the fake identities.
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGreyUtils fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGreyUtils fakeIdentity2];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity2];

  // Sign in to |fakeIdentity1|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI
      signOutWithSignOutConfirmation:SignOutConfirmationNonManagedUser];

  // Sign in with |fakeIdentity2|.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI selectIdentityWithEmail:fakeIdentity2.userEmail];
  [SigninEarlGreyUI confirmSigninConfirmationDialog];

  // Switch Sync account to |fakeIdentity2| should ask whether date should be
  // imported or kept separate. Choose to keep data separate.
  [[EarlGrey selectElementWithMatcher:choiceButtonMatcher]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsImportDataContinueButton()]
      performAction:grey_tap()];

  // Check the signed-in user did change.
  [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity2];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninInteractionControllerAppInterface setActivityIndicatorShown:YES];
}

}  // namespace

// Sign-in interaction tests that work both with Unified Consent enabled or
// disabled.
@interface SigninInteractionControllerTestCase : ChromeTestCase
@end

@implementation SigninInteractionControllerTestCase

- (void)setUp {
  [super setUp];
  // Remove closed tab history to make sure the sign-in promo is always visible
  // in recent tabs.
  [ChromeEarlGrey clearBrowsingHistory];
}

// Tests that opening the sign-in screen from the Settings and signing in works
// correctly when there is already an identity on the device.
- (void)testSignInOneUser {
  // Set up a fake identity.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Check |fakeIdentity| is signed-in.
  [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity];
}

// Tests signing in with one account, switching sync account to a second and
// choosing to keep the browsing data separate during the switch.
- (void)testSignInSwitchAccountsAndKeepDataSeparate {
  ChooseImportOrKeepDataSepareteDialog(SettingsImportDataKeepSeparateButton());
}

// Tests signing in with one account, switching sync account to a second and
// choosing to import the browsing data during the switch.
- (void)testSignInSwitchAccountsAndImportData {
  ChooseImportOrKeepDataSepareteDialog(SettingsImportDataImportButton());
}

// Tests that signing out from the Settings works correctly.
- (void)testSignInDisconnectFromChrome {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  // Sign in to |fakeIdentity|.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithSignOutConfirmation:SignOutConfirmationNonManagedUser];
}

// Tests that signing out of a managed account from the Settings works
// correctly.
- (void)testSignInDisconnectFromChromeManaged {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeManagedIdentity];

  // Sign-in with a managed account.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity isManagedAccount:YES];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithSignOutConfirmation:SignOutConfirmationManagedUser];

  // Check that there is no signed in user.
  [SigninEarlGreyUtils checkSignedOut];
}

// Tests that signing in, tapping the Settings link on the confirmation screen
// and closing the advanced sign-in settings correctly leaves the user signed
// in.
- (void)testSignInOpenSettings {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:YES];

  [[EarlGrey selectElementWithMatcher:SyncSettingsConfirmButton()]
      performAction:grey_tap()];

  // Test sync is on in the settings view.
  id<GREYMatcher> settings_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_SIGN_IN_TO_CHROME_SETTING_SYNC_ON);
  [[EarlGrey selectElementWithMatcher:settings_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Test the user is signed in.
  [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity];
}

// Opens the sign in screen and then cancel it by opening a new tab. Ensures
// that the sign in screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelIdentityPicker {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  // Waits until the UI is fully presented before opening an URL.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Open new tab to cancel sign-in.
  [ChromeEarlGrey simulateExternalAppURLOpening];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:fakeIdentity.userEmail];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Starts an authentication flow and cancel it by opening a new tab. Ensures
// that the authentication flow is correctly canceled and dismissed.
// crbug.com/462202
- (void)testSignInCancelAuthenticationFlow {
  // The ChromeSigninView's activity indicator must be hidden as the import
  // data UI is presented on top of the activity indicator and Earl Grey cannot
  // interact with any UI while an animation is active.
  [SigninInteractionControllerAppInterface setActivityIndicatorShown:NO];

  // Set up the fake identities.
  FakeChromeIdentity* fakeIdentity1 = [SigninEarlGreyUtils fakeIdentity1];
  FakeChromeIdentity* fakeIdentity2 = [SigninEarlGreyUtils fakeIdentity2];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity2];

  // This signs in |fakeIdentity2| first, ensuring that the "Clear Data Before
  // Syncing" dialog is shown during the second sign-in. This dialog will
  // effectively block the authentication flow, ensuring that the authentication
  // flow is always still running when the sign-in is being cancelled.
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity2];

  // Sign out.
  [SigninEarlGreyUI
      signOutWithSignOutConfirmation:SignOutConfirmationNonManagedUser];
  // Sign in with |fakeIdentity1|.
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SecondarySignInButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUI selectIdentityWithEmail:fakeIdentity1.userEmail];
  // The authentication flow is only created when the confirm button is
  // selected. Note that authentication flow actually blocks as the
  // "Clear Browsing Before Syncing" dialog is presented.
  [SigninEarlGreyUI confirmSigninConfirmationDialog];
  // Waits until the merge/delete data panel is shown.
  [[EarlGrey selectElementWithMatcher:SettingsImportDataKeepSeparateButton()]
      assertWithMatcher:grey_interactable()];

  // Open new tab to cancel sign-in.
  [ChromeEarlGrey simulateExternalAppURLOpening];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:fakeIdentity1.userEmail];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGreyUtils checkSignedOut];
  [SigninInteractionControllerAppInterface setActivityIndicatorShown:YES];
}

// Opens the sign in screen from the bookmarks and then cancel it by tapping on
// done. Ensures that the sign in screen is correctly dismissed.
// Regression test for crbug.com/596029.
- (void)testSignInCancelFromBookmarks {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  // Open Bookmarks and tap on Sign In promo button.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Assert sign-in screen was shown.
  [SigninEarlGreyUI selectIdentityWithEmail:fakeIdentity.userEmail];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Open new tab to cancel sign-in.
  [ChromeEarlGrey simulateExternalAppURLOpening];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:fakeIdentity.userEmail];

  // Verifies that the Chrome sign-in view is visible.
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Bookmarks.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BookmarksNavigationBarDoneButton()]
      performAction:grey_tap()];
}

#pragma mark - Dismiss tests

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: user consent.
- (void)testDismissSigninFromSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: setting menu.
// Interrupted at: advanced sign-in.
- (void)testDismissAdvancedSigninSettingsFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromSettings
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: user consent.
- (void)testDismissSigninFromBookmarks {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromBookmarks
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: bookmark view.
// Interrupted at: advanced sign-in.
- (void)testDismissAdvancedSigninBookmarksFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromBookmarks
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: user consent.
- (void)testDismissSigninFromRecentTabs {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: recent tabs.
// Interrupted at: advanced sign-in.
- (void)testDismissSigninFromRecentTabsFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromRecentTabs
                        tapSettingsLink:YES];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: user consent.
- (void)testDismissSigninFromTabSwitcher {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher
                        tapSettingsLink:NO];
}

// Tests to dismiss sign-in by opening an URL from another app.
// Sign-in opened from: tab switcher.
// Interrupted at: advanced sign-in.
- (void)testDismissSigninFromTabSwitcherFromAdvancedSigninSettings {
  [self assertOpenURLWhenSigninFromView:OpenSigninMethodFromTabSwitcher
                        tapSettingsLink:YES];
}

// Verifies that advanced sign-in shows an alert dialog when being swiped to
// dismiss.
- (void)testSwipeDownToCancelAdvancedSignin {
  if (!base::ios::IsRunningOnOrLater(13, 0, 0)) {
    EARL_GREY_TEST_SKIPPED(@"Test disabled on iOS 12 and lower.");
  }
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];
  [self openSigninFromView:OpenSigninMethodFromSettings tapSettingsLink:YES];

  [[EarlGrey selectElementWithMatcher:GoogleServicesSettingsView()]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [[EarlGrey
      selectElementWithMatcher:
          ButtonWithAccessibilityLabel(GetNSString(
              IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_CANCEL_SYNC_BUTTON))]
      performAction:grey_tap()];
  [SigninEarlGreyUtils checkSignedOut];
}

#pragma mark - Utils

// Opens sign-in view.
// |openSigninMethod| is the way to start the sign-in.
// |tapSettingsLink| if YES, the setting link is tapped before opening the URL.
- (void)openSigninFromView:(OpenSigninMethod)openSigninMethod
           tapSettingsLink:(BOOL)tapSettingsLink {
  switch (openSigninMethod) {
    case OpenSigninMethodFromSettings:
      [ChromeEarlGreyUI openSettingsMenu];
      [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
      break;
    case OpenSigninMethodFromBookmarks:
      [ChromeEarlGreyUI openToolsMenu];
      [ChromeEarlGreyUI
          tapToolsMenuButton:chrome_test_util::BookmarksMenuButton()];
      [[EarlGrey selectElementWithMatcher:PrimarySignInButton()]
          performAction:grey_tap()];
      break;
    case OpenSigninMethodFromRecentTabs:
      [ChromeEarlGreyUI openToolsMenu];
      [ChromeEarlGreyUI
          tapToolsMenuButton:chrome_test_util::RecentTabsMenuButton()];
      TapOnPrimarySignInButtonInRecentTabs();
      break;
    case OpenSigninMethodFromTabSwitcher:
      [[EarlGrey selectElementWithMatcher:chrome_test_util::TabGridOpenButton()]
          performAction:grey_tap()];
      [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                              TabGridOtherDevicesPanelButton()]
          performAction:grey_tap()];
      TapOnPrimarySignInButtonInRecentTabs();
      break;
  }
  if (tapSettingsLink) {
    [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
    [SigninEarlGreyUI tapSettingsLink];
  }
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Starts the sign-in workflow, and simulates opening an URL from another app.
// |openSigninMethod| is the way to start the sign-in.
// |tapSettingsLink| if YES, the setting link is tapped before opening the URL.
- (void)assertOpenURLWhenSigninFromView:(OpenSigninMethod)openSigninMethod
                        tapSettingsLink:(BOOL)tapSettingsLink {
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];
  [self openSigninFromView:openSigninMethod tapSettingsLink:tapSettingsLink];
  // Open the URL as if it was opened from another app.
  [ChromeEarlGrey simulateExternalAppURLOpening];

  // Check if the URL was opened.
  const GURL expectedURL("http://www.example.com/");
  GREYAssertEqual(expectedURL, [ChromeEarlGrey webStateVisibleURL],
                  @"Didn't open new tab with example.com.");

  if (tapSettingsLink) {
    // Should be signed in.
    [SigninEarlGreyUtils checkSignedInWithFakeIdentity:fakeIdentity];
  } else {
    // Should be not signed in.
    [SigninEarlGreyUtils checkSignedOut];
  }
}

// Checks that the fake SSO screen shown on adding an account is visible
// onscreen.
- (void)assertFakeSSOScreenIsVisible {
  // Check for the fake SSO screen.
  WaitForMatcher(grey_accessibilityID(kFakeAddAccountViewIdentifier));
  // Close the SSO view controller.
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::ButtonWithAccessibilityLabel(@"Cancel"),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Make sure the SSO view controller is fully removed before ending the test.
  // The tear down needs to remove other view controllers, and it cannot be done
  // during the animation of the SSO view controler.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Tests the "ADD ACCOUNT" button in the identity chooser view controller.
- (void)testAddAccountAutomatically {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PrimarySignInButton()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
  // Tap on "ADD ACCOUNT".
  [SigninEarlGreyUI tapAddAccountButton];

  [self assertFakeSSOScreenIsVisible];
  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Tests that an add account operation triggered from the web is handled.
// Regression test for crbug.com/1054861.
- (void)testSigninAddAccountFromWeb {
  [ChromeEarlGrey simulateAddAccountFromWeb];

  [self assertFakeSSOScreenIsVisible];
}

// Tests to remove the last identity in the identity chooser.
- (void)testRemoveLastAccount {
  // Set up a fake identity.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  // Open the identity chooser.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  WaitForMatcher(identityChooserButtonMatcherWithEmail(fakeIdentity.userEmail));

  // Remove the fake identity.
  [SigninEarlGreyUtils removeFakeIdentity:fakeIdentity];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Check that the identity has been removed.
  [[EarlGrey selectElementWithMatcher:identityChooserButtonMatcherWithEmail(
                                          fakeIdentity.userEmail)]
      assertWithMatcher:grey_notVisible()];
}

// Opens the add account screen and then cancels it by opening a new tab.
// Ensures that the add account screen is correctly dismissed. crbug.com/462200
- (void)testSignInCancelAddAccount {
  // Add an identity to avoid arriving on the Add Account screen when opening
  // sign-in.
  FakeChromeIdentity* fakeIdentity = [SigninEarlGreyUtils fakeIdentity1];
  [SigninEarlGreyUtils addFakeIdentity:fakeIdentity];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];

  // Open Add Account screen.
  id<GREYMatcher> add_account_matcher =
      chrome_test_util::StaticTextWithAccessibilityLabelId(
          IDS_IOS_ACCOUNT_IDENTITY_CHOOSER_ADD_ACCOUNT);
  [[EarlGrey selectElementWithMatcher:add_account_matcher]
      performAction:grey_tap()];
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Open new tab to cancel sign-in.
  [ChromeEarlGrey simulateExternalAppURLOpening];

  // Re-open the sign-in screen. If it wasn't correctly dismissed previously,
  // this will fail.
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SecondarySignInButton()];
  [SigninEarlGreyUI selectIdentityWithEmail:fakeIdentity.userEmail];

  // Verifies that the Chrome sign-in view is visible.
  id<GREYMatcher> signin_matcher = StaticTextWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE);
  [[EarlGrey selectElementWithMatcher:signin_matcher]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Close sign-in screen and Settings.
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SKIP_BUTTON)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

@end
