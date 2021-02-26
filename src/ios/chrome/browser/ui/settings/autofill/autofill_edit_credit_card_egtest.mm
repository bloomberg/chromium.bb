// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/autofill/autofill_app_interface.h"
#import "ios/chrome/browser/ui/settings/autofill/features.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabel;
using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::NavigationBarDoneButton;
using chrome_test_util::PaymentMethodsButton;
using chrome_test_util::TextFieldForCellWithLabelId;

// Tests for Settings Autofill edit credit cards screen.
@interface AutofillEditCreditCardTestCase : ChromeTestCase
@end

namespace {

// Matcher for the 'Nickname' text field in the add credit card view.
id<GREYMatcher> NicknameTextField() {
  return TextFieldForCellWithLabelId(IDS_IOS_AUTOFILL_NICKNAME);
}

// Matcher for the edit button in the navigation bar.
id<GREYMatcher> NavigationBarEditButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_EDIT_BUTTON),
      grey_kindOfClass([UIButton class]),
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
}

}  // namespace

@implementation AutofillEditCreditCardTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      autofill::features::kAutofillEnableCardNicknameManagement);
  return config;
}

- (void)setUp {
  [super setUp];

  [AutofillAppInterface clearCreditCardStore];
  NSString* lastDigits = [AutofillAppInterface saveLocalCreditCard];

  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(lastDigits)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:NavigationBarEditButton()]
      performAction:grey_tap()];
}

- (void)tearDown {
  [AutofillAppInterface clearCreditCardStore];
  [super tearDown];
}

#pragma mark - Test that all fields on the 'Add Credit Card' screen appear

// Tests that editing the credit card nickname is possible.
- (void)testValidNickname {
#if !TARGET_OS_SIMULATOR
  // TODO(crbug.com/1108809): These seem to fail on device only downstream,
  // iOS 12.4 only.
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS12 device.");
  }
#endif

  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_replaceText(@"Nickname")];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(), grey_enabled(),
                                   nil)];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      performAction:grey_tap()];
}

// Tests that invalid nicknames are not allowed when editing a card.
- (void)testInvalidNickname {
#if !TARGET_OS_SIMULATOR
  // TODO(crbug.com/1108809): These seem to fail on device only downstream,
  // iOS 12.4 only.
  if (@available(iOS 13, *)) {
  } else {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS12 device.");
  }
#endif

  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_typeText(@"1233")];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(),
                                   grey_not(grey_enabled()), nil)];
}

// Tests that clearing a nickname is allowed.
// TODO(crbug.com/1108809): Re-enable the test.
- (void)DISABLED_testEmptyNickname {
  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_typeText(@"To be removed")];

  [[EarlGrey selectElementWithMatcher:NicknameTextField()]
      performAction:grey_clearText()];

  [[EarlGrey selectElementWithMatcher:NavigationBarDoneButton()]
      assertWithMatcher:grey_allOf(grey_sufficientlyVisible(), grey_enabled(),
                                   nil)];
}

@end
