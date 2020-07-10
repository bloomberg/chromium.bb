// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#include "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;

namespace {

// Expectation of how the saved Autofill credit card looks like, a map from cell
// name IDs to expected contents.
struct DisplayStringIDToExpectedResult {
  int display_string_id;
  NSString* expected_result;
};

const DisplayStringIDToExpectedResult kExpectedFields[] = {
    {IDS_IOS_AUTOFILL_CARDHOLDER, @"Test User"},
    {IDS_IOS_AUTOFILL_CARD_NUMBER, @"4111111111111111"},
    {IDS_IOS_AUTOFILL_EXP_MONTH, @"11"},
    {IDS_IOS_AUTOFILL_EXP_YEAR, @"2022"}};

NSString* const kCreditCardLabelTemplate = @"Test User, %@";

// Return the edit button from the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}


// Matcher for the Delete button in the list view, located at the bottom of the
// screen.
id<GREYMatcher> BottomToolbar() {
  return grey_accessibilityID(kAutofillPaymentMethodsToolbarId);
}

}  // namespace

// Various tests for the Autofill credit cards section of the settings.
@interface AutofillCreditCardSettingsTestCase : ChromeTestCase
@end

@implementation AutofillCreditCardSettingsTestCase

- (void)setUp {
  [super setUp];

  [AutofillAppInterface clearCreditCardStore];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [super tearDown];
}

// Returns the label for |creditCard| in the settings page for Autofill credit
// cards.
- (NSString*)creditCardLabel:(NSString*)lastDigits {
  return [NSString stringWithFormat:kCreditCardLabelTemplate, lastDigits];
}

// Helper to open the settings page for Autofill credit cards.
- (void)openCreditCardsSettings {
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          l10n_util::GetNSString(
                                              IDS_AUTOFILL_PAYMENT_METHODS))]
      performAction:grey_tap()];
}

// Helper to open the settings page for the Autofill credit card with |label|.
- (void)openEditCreditCard:(NSString*)label {
  [self openCreditCardsSettings];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(label)]
      performAction:grey_tap()];
}

// Helper to open the settings page for the Autofill credit card list in edit
// mode.
- (void)openCreditCardListInEditMode {
  [self openCreditCardsSettings];

  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

// Close the settings.
- (void)exitSettingsMenu {
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
  // Wait for UI components to finish loading.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];
}

// Test that the page for viewing Autofill credit card details is as expected.
- (void)testCreditCardViewPage {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openEditCreditCard:[self creditCardLabel:lastDigits]];

  // Check that all fields and values match the expectations.
  for (const DisplayStringIDToExpectedResult& expectation : kExpectedFields) {
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityLabel([NSString
                       stringWithFormat:@"%@, %@",
                                        l10n_util::GetNSString(
                                            expectation.display_string_id),
                                        expectation.expected_result])]
        assertWithMatcher:grey_notNil()];
  }

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for viewing Autofill credit card details is accessible.
- (void)testAccessibilityOnCreditCardViewPage {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openEditCreditCard:[self creditCardLabel:lastDigits]];

  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Test that the page for editing Autofill credit card details is accessible.
- (void)testAccessibilityOnCreditCardEditPage {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openEditCreditCard:[self creditCardLabel:lastDigits]];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];

  // Go back to the list view page.
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      performAction:grey_tap()];

  [self exitSettingsMenu];
}

// Checks that the Autofill credit cards list view is in edit mode and the
// Autofill credit cards switch is disabled.
- (void)testListViewEditMode {
  [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardsSettings];

  // Switch on edit mode.
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];

  // Check the Autofill credit card switch is disabled.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                          kAutofillCreditCardSwitchViewId, YES,
                                          NO)] assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the Autofill credit card switch can be toggled on/off and the
// list of Autofill credit cards is not affected by it.
- (void)testToggleCreditCardSwitch {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardsSettings];

  // Toggle the Autofill credit cards switch off.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAutofillCreditCardSwitchViewId, YES, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(NO)];

  // Expect Autofill credit cards to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      assertWithMatcher:grey_notNil()];

  // Toggle the Autofill credit cards switch back on.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::SettingsSwitchCell(
                                   kAutofillCreditCardSwitchViewId, NO, YES)]
      performAction:chrome_test_util::TurnSettingsSwitchOn(YES)];

  // Expect Autofill credit cards to remain visible.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      assertWithMatcher:grey_notNil()];

  [self exitSettingsMenu];
}

// Checks that the toolbar appears in edit mode once a card is selected and
// disappears when a card is deselected.
- (void)testToolbarInEditMode {
  if ([ChromeEarlGrey isSettingsAddPaymentMethodEnabled]) {
    // The toolbar is always displayed when the AddPayment feature is enabled.
    EARL_GREY_TEST_SKIPPED(
        @"This test makes sense only when there is no Add Payment button.");
  }
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardListInEditMode];

  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Checks the toolbar buttons in the edit mode of the list of credit cards. The
// delete button should appear on selecting a card and be removed when no card
// is selected. There should be no 'Add payment method' button if the
// kSettingsAddPaymentMethod flag is not enabled.
- (void)testToolbarButtonsInEditMode {
  if ([ChromeEarlGrey isSettingsAddPaymentMethodEnabled]) {
    // The toolbar is always displayed when the AddPayment feature is enabled.
    EARL_GREY_TEST_SKIPPED(
        @"This test makes sense only when there is no Add Payment button.");
  }

  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardListInEditMode];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::AddPaymentMethodButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::AddPaymentMethodButton()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_nil()];
}

// Checks that the toolbar always appears in edit mode when the 'Add Payment
// method' feature is enabled.
- (void)testToolbarInEditModeAddPaymentMethodFeatureEnabled {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kSettingsAddPaymentMethod}
                                  disabled:{}
                            relaunchPolicy:NoForceRelaunchAndResetState];
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardListInEditMode];

  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BottomToolbar()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks the 'Add Payment Method' button is always visible when the feature is
// enabled and directs a user to the Add Payent method view.
- (void)testToolbarAddPaymentMethodButtonFeatureEnabled {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kSettingsAddPaymentMethod}
                                  disabled:{}
                            relaunchPolicy:NoForceRelaunchAndResetState];
  [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardListInEditMode];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::AddPaymentMethodButton()]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::AddPaymentMethodButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::AddCreditCardView()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Checks the 'Delete' button is always visible when the feature is displayed.
// The button is enabled when a card is selected and disabled when a card is not
// selected.
- (void)testToolbarDeleteButtonWithAddPaymentMethodFeatureEnabled {
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kSettingsAddPaymentMethod}
                                  disabled:{}
                            relaunchPolicy:NoForceRelaunchAndResetState];

  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
  [self openCreditCardListInEditMode];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_not(grey_enabled())];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_enabled()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_not(grey_enabled())];
}

// Checks that deleting a card exits from edit mode.
- (void)testDeletingCreditCard {
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];
  [self openCreditCardListInEditMode];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                          [self creditCardLabel:lastDigits])]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SettingsBottomToolbarDeleteButton()]
      assertWithMatcher:grey_nil()];
  // If the done button is nil it is no longer in edit mode.
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_nil()];
}

@end
