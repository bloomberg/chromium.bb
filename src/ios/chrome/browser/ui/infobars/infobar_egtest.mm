// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/infobars/core/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_manager_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
// TODO(crbug.com/1015113): The EG2 macro is breaking indexing for some reason
// without the trailing semicolon.  For now, disable the extra semi warning
// so Xcode indexing works for the egtest.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(InfobarManagerAppInterface);
#endif  // defined(CHROME_EARL_GREY_2)

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Verifies that a single TestInfoBar with |message| is either present or absent
// on the current tab.
void VerifyTestInfoBarVisibleForCurrentTab(bool visible, NSString* message) {
  NSString* condition_name =
      visible ? @"Waiting for infobar to show" : @"Waiting for infobar to hide";
  id<GREYMatcher> expected_visibility = visible ? grey_notNil() : grey_nil();
#if defined(CHROME_EARL_GREY_1)
  CFTimeInterval kTimeout = 4.0;
  [[GREYCondition
      conditionWithName:condition_name
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey
                        selectElementWithMatcher:
                            chrome_test_util::StaticTextWithAccessibilityLabel(
                                message)] assertWithMatcher:expected_visibility
                                                      error:&error];
                    return error == nil;
                  }] waitWithTimeout:kTimeout];
#elif defined(CHROME_EARL_GREY_2)
  BOOL bannerShown = WaitUntilConditionOrTimeout(
      kInfobarBannerDefaultPresentationDurationInSeconds, ^{
        NSError* error = nil;
        [[EarlGrey
            selectElementWithMatcher:grey_allOf(
                                         grey_accessibilityID(
                                             kInfobarBannerViewIdentifier),
                                         grey_accessibilityLabel(message), nil)]
            assertWithMatcher:expected_visibility
                        error:&error];
        return error == nil;
      });

  GREYAssertTrue(bannerShown, condition_name);
#else
#error Must define either CHROME_EARL_GREY_1 or CHROME_EARL_GREY_2.
#endif
}

}  // namespace

// Tests functionality related to infobars.
@interface InfobarTestCase : ChromeTestCase
@end

@implementation InfobarTestCase {
  base::test::ScopedFeatureList _featureList;
}

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kIOSInfobarUIReboot);
  config.features_disabled.push_back(kInfobarUIRebootOnlyiOS13);
  return config;
}

// Tests that page infobars don't persist on navigation.
- (void)testInfobarsDismissOnNavigate {
  // Open a new tab and navigate to the test page.
  const GURL testURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Infobar Message
  NSString* infoBarMessage = @"TestInfoBar";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:infoBarMessage],
                 @"Failed to add infobar to test tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Navigate to a different page.  Verify that the infobar is dismissed and no
  // longer visible on screen.
  [ChromeEarlGrey loadURL:GURL(url::kAboutBlankURL)];
  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");
}

// Tests that page infobars persist only on the tabs they are opened on, and
// that navigation in other tabs doesn't affect them.
- (void)testInfobarTabSwitch {
  const GURL destinationURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  const GURL ponyURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");

  // Create the first tab and navigate to the test page.
  [ChromeEarlGrey loadURL:destinationURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Infobar Message
  NSString* infoBarMessage = @"TestInfoBar";

  // Create the second tab, navigate to the test page, and add the test infobar.
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey loadURL:ponyURL];
  [ChromeEarlGrey waitForMainTabCount:2];
  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:infoBarMessage],
                 @"Failed to add infobar to second tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Switch back to the first tab and make sure no infobar is visible.
  [ChromeEarlGrey selectTabAtIndex:0U];
  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");

  // Navigate to a different URL in the first tab, to verify that this
  // navigation does not hide the infobar in the second tab.
  [ChromeEarlGrey loadURL:ponyURL];

  // Close the first tab.  Verify that there is only one tab remaining.
  [ChromeEarlGrey closeCurrentTab];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that the Infobar dissapears once the "OK" button is tapped.
- (void)testInfobarButtonDismissal {
  // Open a new tab and navigate to the test page.
  const GURL testURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Infobar Message
  NSString* infoBarMessage = @"TestInfoBar";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:infoBarMessage],
                 @"Failed to add infobar to test tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Tap on "OK" which should dismiss the Infobar.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_buttonTitle(@"OK"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  VerifyTestInfoBarVisibleForCurrentTab(false, infoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");
}

// Tests adding an Infobar on top of an existing one.
- (void)testInfobarTopMostVisible {
// Turn on Messages UI.
#if defined(CHROME_EARL_GREY_1)
  _featureList.InitWithFeatures(
      /*enabled_features=*/{kIOSInfobarUIReboot},
      /*disabled_features=*/{kInfobarUIRebootOnlyiOS13});
#endif

  // Open a new tab and navigate to the test page.
  const GURL testURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // First Infobar Message
  NSString* firstInfoBarMessage = @"TestFirstInfoBar";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:firstInfoBarMessage],
                 @"Failed to add infobar to test tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, firstInfoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Second Infobar Message
  NSString* secondInfoBarMessage = @"TestSecondInfoBar";

  // Add a second test infobar to the current tab. Verify that the infobar is
  // present in the model, and that only this second infobar is now visible on
  // screen.
  GREYAssertTrue(
      [InfobarManagerAppInterface
          addTestInfoBarToCurrentTabWithMessage:secondInfoBarMessage],
      @"Failed to add infobar to test tab.");
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:2],
                 @"Incorrect number of infobars.");
  VerifyTestInfoBarVisibleForCurrentTab(false, firstInfoBarMessage);
  VerifyTestInfoBarVisibleForCurrentTab(true, secondInfoBarMessage);
  // Confirm infobars are destroyed after their banners are dismissed.
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");
}

// Tests that a taller Infobar layout is correct and the OK button is tappable.
- (void)testInfobarTallerLayout {
  // Turn on Messages UI.
#if defined(CHROME_EARL_GREY_1)
  _featureList.InitWithFeatures(
      /*enabled_features=*/{kIOSInfobarUIReboot},
      /*disabled_features=*/{kInfobarUIRebootOnlyiOS13});
#endif

  // Open a new tab and navigate to the test page.
  const GURL testURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/pony.html");
  [ChromeEarlGrey loadURL:testURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Infobar Message
  NSString* firstInfoBarMessage =
      @"This is a really long message that will cause this infobar height to "
      @"increase since Confirm Infobar heights changes depending on its "
      @"message.";

  // Add a test infobar to the current tab. Verify that the infobar is present
  // in the model and that the infobar view is visible on screen.
  GREYAssertTrue([InfobarManagerAppInterface
                     addTestInfoBarToCurrentTabWithMessage:firstInfoBarMessage],
                 @"Failed to add infobar to test tab.");
  VerifyTestInfoBarVisibleForCurrentTab(true, firstInfoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:1],
                 @"Incorrect number of infobars.");

  // Dismiss the Infobar.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_buttonTitle(@"OK"),
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  VerifyTestInfoBarVisibleForCurrentTab(false, firstInfoBarMessage);
  GREYAssertTrue([InfobarManagerAppInterface verifyInfobarCount:0],
                 @"Incorrect number of infobars.");
}
@end
