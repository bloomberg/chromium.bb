// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/authentication/views/views_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/scoped_eg_synchronization_disabler.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service_constants.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::PrimarySignInButton;
using chrome_test_util::SecondarySignInButton;
using chrome_test_util::SettingsAccountButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SignOutAccountsButton;

namespace {

// Closes the managed account dialog, if |fakeIdentity| is a managed account.
void CloseSigninManagedAccountDialogIfAny(FakeChromeIdentity* fakeIdentity) {
  if (![fakeIdentity.userEmail hasSuffix:ios::kManagedIdentityEmailSuffix]) {
    return;
  }
  // Synchronization off due to an infinite spinner, in the user consent view,
  // under the managed consent dialog. This spinner is started by the sign-in
  // process.
  ScopedSynchronizationDisabler disabler;
  id<GREYMatcher> acceptButton = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_MANAGED_SIGNIN_ACCEPT_BUTTON];
  [ChromeEarlGrey waitForMatcher:acceptButton];
  [[EarlGrey selectElementWithMatcher:acceptButton] performAction:grey_tap()];
}

}  // namespace

@implementation SigninEarlGreyUI

+ (void)signinWithFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  [self signinWithFakeIdentity:fakeIdentity enableSync:YES];
}

+ (void)signinWithFakeIdentity:(FakeChromeIdentity*)fakeIdentity
                    enableSync:(BOOL)enableSync {
  [SigninEarlGrey addFakeIdentity:fakeIdentity];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI
      tapSettingsMenuButton:chrome_test_util::PrimarySignInButton()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kIdentityButtonControlIdentifier)]
      performAction:grey_tap()];
  [self selectIdentityWithEmail:fakeIdentity.userEmail];
  [self tapSigninConfirmationDialog];
  CloseSigninManagedAccountDialogIfAny(fakeIdentity);

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];

  // Sync utilities require sync to be initialized in order to perform
  // operations on the Sync server.
  [ChromeEarlGrey waitForSyncInitialized:YES syncTimeout:10.0];

  if (!enableSync) {
    [ChromeEarlGrey stopSync];
    [ChromeEarlGrey revokeSyncConsent];
    // Ensure that Sync preferences do not take into account the first setup.
    [ChromeEarlGrey clearSyncFirstSetupComplete];
  }
}

+ (void)signOut {
  NSString* primaryAccountEmail =
      [SigninEarlGreyAppInterface primaryAccountEmail];
  GREYAssert(![primaryAccountEmail hasSuffix:ios::kManagedIdentityEmailSuffix],
             @"Managed account must clear data on signout");
  [self signOutWithButton:SignOutAccountsButton()
      confirmationLabelID:IDS_IOS_DISCONNECT_DIALOG_CONTINUE_BUTTON_MOBILE];
}

+ (void)signOutAndClearDataFromDevice {
  [self signOutWithButton:
            grey_accessibilityID(
                kSettingsAccountsTableViewSignoutAndClearDataCellId)
      confirmationLabelID:IDS_IOS_DISCONNECT_DIALOG_CONTINUE_AND_CLEAR_MOBILE];
}

+ (void)signOutWithConfirmationChoice:(SignOutConfirmationChoice)confirmation {
  int confirmationLabelID = 0;
  switch (confirmation) {
    case SignOutConfirmationChoiceClearData:
      confirmationLabelID = IDS_IOS_SIGNOUT_DIALOG_CLEAR_DATA_BUTTON;
      break;
    case SignOutConfirmationChoiceKeepData:
      confirmationLabelID = IDS_IOS_SIGNOUT_DIALOG_KEEP_DATA_BUTTON;
      break;
    case SignOutConfirmationChoiceNotSyncing:
      confirmationLabelID = IDS_IOS_SIGNOUT_DIALOG_SIGN_OUT_BUTTON;
      break;
  }
  [self signOutWithButton:SignOutAccountsButton()
      confirmationLabelID:confirmationLabelID];
}

+ (void)selectIdentityWithEmail:(NSString*)userEmail {
  // Assumes that the identity chooser is visible.
  [[EarlGrey
      selectElementWithMatcher:[SigninEarlGreyAppInterface
                                   identityCellMatcherForEmail:userEmail]]
      performAction:grey_tap()];
}

+ (void)tapSettingsLink {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"settings"),
                                          grey_accessibilityTrait(
                                              UIAccessibilityTraitLink),
                                          nil)] performAction:grey_tap()];
}

+ (void)tapSigninConfirmationDialog {
  // To confirm the dialog, the scroll view content has to be scrolled to the
  // bottom to transform "MORE" button into the validation button.
  // EarlGrey fails to scroll to the bottom, using grey_scrollToContentEdge(),
  // if the scroll view doesn't bounce and by default a scroll view doesn't
  // bounce when the content fits into the scroll view (the scroll never ends).
  // To test if the content fits into the scroll view,
  // ContentViewSmallerThanScrollView() matcher is used on the signin scroll
  // view.
  // If the matcher fails, then the scroll view should be scrolled to the
  // bottom.
  // Once to the bottom, the consent can be confirmed.
  [ChromeEarlGreyUI waitForAppToIdle];
  id<GREYMatcher> confirmationScrollViewMatcher =
      grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
      assertWithMatcher:chrome_test_util::ContentViewSmallerThanScrollView()
                  error:&error];
  if (error) {
    [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }
  id<GREYMatcher> buttonMatcher = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON];
  [[EarlGrey selectElementWithMatcher:buttonMatcher] performAction:grey_tap()];
}

+ (void)tapAddAccountButton {
  id<GREYMatcher> confirmationScrollViewMatcher =
      grey_accessibilityID(kUnifiedConsentScrollViewIdentifier);
  [ChromeEarlGreyUI waitForAppToIdle];
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
      assertWithMatcher:chrome_test_util::ContentViewSmallerThanScrollView()
                  error:&error];
  if (error) {
    // If the consent is bigger than the scroll view, the primary button should
    // be "MORE".
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::ButtonWithAccessibilityLabelId(
                       IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SCROLL_BUTTON)]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:confirmationScrollViewMatcher]
        performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];
  }
  id<GREYMatcher> buttonMatcher = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT];
  [[EarlGrey selectElementWithMatcher:buttonMatcher] performAction:grey_tap()];
}

+ (void)verifySigninPromoVisibleWithMode:(SigninPromoViewMode)mode {
  [self verifySigninPromoVisibleWithMode:mode closeButton:YES];
}

+ (void)verifySigninPromoVisibleWithMode:(SigninPromoViewMode)mode
                             closeButton:(BOOL)closeButton {
  [ChromeEarlGreyUI waitForAppToIdle];

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];

  switch (mode) {
    case SigninPromoViewModeNoAccounts:
    case SigninPromoViewModeSyncWithPrimaryAccount:
      [[EarlGrey
          selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                              grey_sufficientlyVisible(), nil)]
          assertWithMatcher:grey_nil()];
      break;
    case SigninPromoViewModeSigninWithAccount:
      // TODO(crbug.com/1210846): Determine when the SecondarySignInButton
      // should be present and assert that.
      break;
  }

  if (closeButton) {
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                kSigninPromoCloseButtonId),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()];
  }
}

+ (void)verifySigninPromoNotVisible {
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityID(kSigninPromoViewId),
                                   grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(SecondarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_nil()];
}

+ (void)openRemoveAccountConfirmationDialogWithFakeIdentity:
    (FakeChromeIdentity*)fakeIdentity {
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                   l10n_util::GetNSString(
                                       IDS_IOS_REMOVE_GOOGLE_ACCOUNT_TITLE))]
      performAction:grey_tap()];
}

+ (void)openMyGoogleDialogWithFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          fakeIdentity.userEmail)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 ButtonWithAccessibilityLabel(l10n_util::GetNSString(
                     IDS_IOS_MANAGE_YOUR_GOOGLE_ACCOUNT_TITLE))]
      performAction:grey_tap()];
  [ChromeEarlGreyUI waitForAppToIdle];
}

+ (void)tapRemoveAccountFromDeviceWithFakeIdentity:
    (FakeChromeIdentity*)fakeIdentity {
  [self openRemoveAccountConfirmationDialogWithFakeIdentity:fakeIdentity];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          l10n_util::GetNSString(
                                              IDS_IOS_REMOVE_ACCOUNT_LABEL))]
      performAction:grey_tap()];
  // Wait until the account is removed.
  [ChromeEarlGreyUI waitForAppToIdle];
}

+ (void)tapPrimarySignInButtonInRecentTabs {
  [ChromeEarlGreyUI openToolsMenu];
  [ChromeEarlGreyUI
      tapToolsMenuButton:chrome_test_util::RecentTabsMenuButton()];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)
      onElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTableViewControllerAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

+ (void)tapPrimarySignInButtonInTabSwitcher {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          TabGridOtherDevicesPanelButton()]
      performAction:grey_tap()];
  // The start point needs to avoid the "Done" bar on iPhone, in order to catch
  // the table view and scroll.
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(PrimarySignInButton(),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollToContentEdgeWithStartPoint(
                               kGREYContentEdgeBottom, 0.5, 0.5)
      onElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kRecentTabsTableViewControllerAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
}

+ (void)verifyWebSigninIsVisible:(BOOL)isVisible {
  id<GREYMatcher> visibilityMatcher =
      isVisible ? grey_sufficientlyVisible() : grey_notVisible();
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kWebSigninAccessibilityIdentifier)]
      assertWithMatcher:visibilityMatcher];
}

#pragma mark - Private

+ (void)signOutWithButton:(id<GREYMatcher>)buttonMatcher
      confirmationLabelID:(int)confirmationLabelID {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsAccountButton()];

  [ChromeEarlGreyUI tapAccountsMenuButton:buttonMatcher];
  id<GREYMatcher> confirmationButtonMatcher = [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:confirmationLabelID];
  [[EarlGrey selectElementWithMatcher:grey_allOf(confirmationButtonMatcher,
                                                 grey_not(buttonMatcher), nil)]
      performAction:grey_tap()];
  // Wait until the user is signed out.
  [ChromeEarlGreyUI waitForAppToIdle];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  [SigninEarlGrey verifySignedOut];
}

@end
