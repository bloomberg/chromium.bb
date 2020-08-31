// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/ios/wait_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Pauses until the history label has disappeared.  History should not show on
// incognito.
BOOL WaitForHistoryToDisappear() {
  return [[GREYCondition
      conditionWithName:@"Wait for history to disappear"
                  block:^BOOL {
                    NSError* error = nil;
                    NSString* history =
                        l10n_util::GetNSString(IDS_HISTORY_SHOW_HISTORY);
                    [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(
                                                            history)]
                        assertWithMatcher:grey_notVisible()
                                    error:&error];
                    return error == nil;
                  }] waitWithTimeout:base::test::ios::kWaitForUIElementTimeout];
}

}  // namespace

@interface NewTabPageTestCase : ChromeTestCase
@end

@implementation NewTabPageTestCase

#pragma mark - Tests

// Tests that all items are accessible on the most visited page.
- (void)testAccessibilityOnMostVisited {
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
}

// Tests that the NTP is still displayed after loading an invalid URL.
- (void)testNTPStayForInvalidURL {
// TODO(crbug.com/1067813): Test won't pass on iPad device.
#if !TARGET_IPHONE_SIMULATOR
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_SKIPPED(@"This test doesn't pass on iPad device.");
  }
#endif
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      performAction:grey_typeText(@"file://\n")];

  // Make sure that the URL disappeared.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText("file://")]
      assertWithMatcher:grey_nil()];

  // Check that the NTP is still displayed (because the fake omnibox is here).
  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that all items are accessible on the incognito page.
- (void)testAccessibilityOnIncognitoTab {
  [ChromeEarlGrey openNewIncognitoTab];
  GREYAssert(WaitForHistoryToDisappear(), @"History did not disappear.");
  [ChromeEarlGrey verifyAccessibilityForCurrentScreen];
  [ChromeEarlGrey closeAllIncognitoTabs];
}

@end
