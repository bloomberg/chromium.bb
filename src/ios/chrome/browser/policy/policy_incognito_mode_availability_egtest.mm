// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/earl_grey_test.h"

#include "base/json/json_string_value_serializer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/policy/policy_constants.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#include "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ToolsMenuView;

namespace {

// Values of the incognito mode availability.
enum class IncognitoAvailability {
  kAvailable = 0,
  kDisabled = 1,
  kOnly = 2,
  kMaxValue = kOnly,
};

// Sets the incognito mode availability.
void SetIncognitoAvailabiliy(IncognitoAvailability availability) {
  [PolicyAppInterface
      setPolicyValue:[NSString stringWithFormat:@"%d",
                                                static_cast<int>(availability),
                                                nil]
              forKey:base::SysUTF8ToNSString(
                         policy::key::kIncognitoModeAvailability)];
}

// Returns a matcher for the tab grid button.
id<GREYMatcher> TabGridButton() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_IOS_TOOLBAR_SHOW_TABS);
}

// Tests the enabled state of an item.
// |parentMatcher| is the container matcher of the |item|.
// |availability| is the expected availability.
void AssertItemEnabledState(id<GREYMatcher> item,
                            id<GREYMatcher> parentMatcher,
                            bool enabled) {
  id<GREYMatcher> enabledMatcher =
      [ChromeEarlGrey isNewOverflowMenuEnabled]
          // TODO(crbug.com/1285974): grey_userInteractionEnabled doesn't work
          // for SwiftUI views.
          ? grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled))
          : grey_userInteractionEnabled();
  [[[EarlGrey selectElementWithMatcher:item]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown,
                                                  /*amount=*/200)
      onElementWithMatcher:parentMatcher]
      assertWithMatcher:enabled ? enabledMatcher
                                : grey_accessibilityTrait(
                                      UIAccessibilityTraitNotEnabled)];
}

}  // namespace

// Test case to verify that the IncognitoModeAvailability policy is set and
// respected.
@interface PolicyIncognitoModeAvailabilityTestCase : ChromeTestCase
@end

@implementation PolicyIncognitoModeAvailabilityTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)tearDown {
  [super tearDown];
  // Close the popup menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
}

// When the IncognitoModeAvailability policy is set to available, the tools
// menu item "New Tab" and "New Incognito Tab" should be enabled.
- (void)testToolsMenuWhenIncognitoAvailable {
  SetIncognitoAvailabiliy(IncognitoAvailability::kAvailable);
  [ChromeEarlGreyUI openToolsMenu];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         ToolsMenuView(), /*enabled=*/YES);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         ToolsMenuView(), /*enabled=*/YES);
}

// When the IncognitoModeAvailability policy is set to disabled, the tools menu
// item "New Incognito Tab" should be disabled.
- (void)testToolsMenuWhenIncognitoDisabled {
  SetIncognitoAvailabiliy(IncognitoAvailability::kDisabled);
  [ChromeEarlGreyUI openToolsMenu];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         ToolsMenuView(), /*enabled=*/YES);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         ToolsMenuView(), /*enabled=*/NO);
}

// When the IncognitoModeAvailability policy is set to forced, the tools menu
// item "New Tab" should be disabled.
- (void)testToolsMenuWhenIncognitoOnly {
  SetIncognitoAvailabiliy(IncognitoAvailability::kOnly);
  [ChromeEarlGreyUI openToolsMenu];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         ToolsMenuView(), /*enabled=*/NO);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         ToolsMenuView(), /*enabled=*/YES);
}

// When the IncognitoModeAvailability policy is set to available, the "New Tab"
// and "New Incognito Tab" items should be enabled in the popup menu triggered
// by long-pressing the tab grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoAvailable {
  SetIncognitoAvailabiliy(IncognitoAvailability::kAvailable);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         grey_accessibilityID(kPopupMenuTabGridMenuTableViewId),
                         /*enabled=*/YES);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         grey_accessibilityID(kPopupMenuTabGridMenuTableViewId),
                         /*enabled=*/YES);
}

// When the IncognitoModeAvailability policy is set to disabled, the "New
// Incognito Tab" item should be disabled in the popup menu triggered by
// long-pressing the tab grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoDisabled {
  SetIncognitoAvailabiliy(IncognitoAvailability::kDisabled);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         grey_accessibilityID(kPopupMenuTabGridMenuTableViewId),
                         /*enabled=*/YES);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         grey_accessibilityID(kPopupMenuTabGridMenuTableViewId),
                         /*enabled=*/NO);
}

// When the IncognitoModeAvailability policy is set to forced, the "New Tab"
// item should be disabled in the popup menu triggered by long-pressing the tab
// grid button.
- (void)testTabGridButtonLongPressMenuWhenIncognitoOnly {
  SetIncognitoAvailabiliy(IncognitoAvailability::kOnly);
  // Long press the tab grid button.
  [[EarlGrey selectElementWithMatcher:TabGridButton()]
      performAction:grey_longPress()];

  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewTabId),
                         grey_accessibilityID(kPopupMenuTabGridMenuTableViewId),
                         NO);
  AssertItemEnabledState(grey_accessibilityID(kToolsMenuNewIncognitoTabId),
                         grey_accessibilityID(kPopupMenuTabGridMenuTableViewId),
                         YES);
}

// TODO(crbug.com/1165655): Add test to new tab long-press menu.

@end
