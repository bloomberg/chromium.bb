// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey_ui_test_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;

namespace {

// Matcher for the feed header menu button.
id<GREYMatcher> FeedMenuButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_DISCOVER_FEED_MENU_ACCESSIBILITY_LABEL);
}
// Matcher for the Turn Off menu item in the feed menu.
id<GREYMatcher> TurnOffFeedMenuItem() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM);
}
// Matcher for the Learn More menu item in the feed menu.
id<GREYMatcher> LearnMoreFeedMenuItem() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM);
}
// Matcher for the Manage menu item in the feed menu.
id<GREYMatcher> ManageFeedMenuItem() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ITEM);
}

// Dismisses the feed menu.
void DismissSignOut() {
  if ([ChromeEarlGrey isIPadIdiom]) {
    // Tap the tools menu to dismiss the popover.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
        performAction:grey_tap()];
  }
}

}  // namespace

@interface FeedManagementTestCase : ChromeTestCase
@end

@implementation FeedManagementTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kEnableWebChannels);
  return config;
}

- (void)testSignedOutOpenAndCloseFeedMenu {
  [[EarlGrey selectElementWithMatcher:FeedMenuButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:TurnOffFeedMenuItem()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:LearnMoreFeedMenuItem()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:ManageFeedMenuItem()]
      assertWithMatcher:grey_nil()];

  DismissSignOut();
}

- (void)testSignedInOpenAndCloseFeedMenu {
  // Sign into a fake identity.
  FakeChromeIdentity* fakeIdentity1 = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  [[EarlGrey selectElementWithMatcher:FeedMenuButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:ManageFeedMenuItem()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:TurnOffFeedMenuItem()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:LearnMoreFeedMenuItem()]
      assertWithMatcher:grey_notNil()];

  DismissSignOut();
}

- (void)testOpenFeedManagementSurface {
  // Sign into a fake identity.
  FakeChromeIdentity* fakeIdentity1 = [FakeChromeIdentity fakeIdentity1];
  [SigninEarlGrey addFakeIdentity:fakeIdentity1];
  [SigninEarlGreyUI signinWithFakeIdentity:fakeIdentity1];

  [[EarlGrey selectElementWithMatcher:FeedMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ManageFeedMenuItem()]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarDoneButton()]
      performAction:grey_tap()];
}

@end
