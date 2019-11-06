// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/public/test/element_selector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@class Tab;

using chrome_test_util::GetCurrentWebState;
using chrome_test_util::OpenLinkInNewTabButton;
using web::WebViewInWebState;

namespace {

// This test uses test pages from in //ios/testing/data/http_server_files/.

// URL for a test page that contains a link.
const char kLinksTestURL1[] = "/links.html";

// Some text in |kLinksTestURL1|.
const char kLinksTestURL1Text[] = "Normal Link";

// ID of the <a> link in |kLinksTestURL1|.
const char kLinkSelectorID[] = "normal-link";

// URL for a different test page.
const char kLinksTestURL2[] = "/destination.html";

// Some text in |kLinksTestURL2|.
const char kLinksTestURL2Text[] = "arrived";

}  // namespace

// Tests the order in which new tabs are created.
@interface TabOrderTestCase : ChromeTestCase
@end

@implementation TabOrderTestCase

// Tests that new tabs are always inserted after their parent tab.
- (void)testChildTabOrdering {
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL URL1 = self.testServer->GetURL(kLinksTestURL1);

  // Create a tab that will act as the parent tab.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:URL1]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kLinksTestURL1Text]);
  Tab* parentTab = chrome_test_util::GetCurrentTab();

  // Child tab should be inserted after the parent.
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkSelectorID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:2U]);
  Tab* childTab1 = chrome_test_util::GetNextTab();

  // New child tab should be inserted AFTER |childTab1|.
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkSelectorID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:3U]);
  GREYAssertEqual(childTab1, chrome_test_util::GetNextTab(),
                  @"Unexpected next tab");

  // Navigate the parent tab away and again to |kLinksTestURL1| to break
  // grouping with the current child tabs. Total number of tabs should not
  // change.
  const GURL URL2 = self.testServer->GetURL(kLinksTestURL2);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:URL2]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kLinksTestURL2Text]);
  GREYAssertEqual(3U, [ChromeEarlGrey mainTabCount],
                  @"Unexpected number of tabs");

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:URL1]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kLinksTestURL1Text]);
  GREYAssertEqual(3U, [ChromeEarlGrey mainTabCount],
                  @"Unexpected number of tabs");

  // New child tab should be inserted BEFORE |childTab1|.
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkSelectorID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:4U]);
  Tab* childTab3 = chrome_test_util::GetNextTab();
  GREYAssertNotEqual(childTab1, childTab3, @"Unexpected next tab");

  // New child tab should be inserted AFTER |childTab3|.
  [[EarlGrey selectElementWithMatcher:WebViewInWebState(GetCurrentWebState())]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector selectorWithElementID:kLinkSelectorID],
                        true /* menu should appear */)];
  [[EarlGrey selectElementWithMatcher:OpenLinkInNewTabButton()]
      performAction:grey_tap()];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:5U]);
  GREYAssertEqual(childTab3, chrome_test_util::GetNextTab(),
                  @"Unexpected next tab");

  // Verify that |childTab1| is now at index 3.
  [ChromeEarlGrey selectTabAtIndex:3];
  GREYAssertEqual(childTab1, chrome_test_util::GetCurrentTab(),
                  @"Unexpected current tab");

  // Add a non-owned tab. It should be added at the end and marked as the
  // current tab. Next tab should wrap back to index 0, the original parent tab.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForMainTabCount:6U]);
  GREYAssertEqual(parentTab, chrome_test_util::GetNextTab(),
                  @"Unexpected next tab");

  // Verify that |anotherTab| is at index 5.
  Tab* anotherTab = chrome_test_util::GetCurrentTab();
  [ChromeEarlGrey selectTabAtIndex:5];
  GREYAssertEqual(anotherTab, chrome_test_util::GetCurrentTab(),
                  @"Unexpected current tab");
}

@end
