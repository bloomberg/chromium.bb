// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#include <map>
#include <memory>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/settings/settings_app_interface.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#if defined(CHROME_EARL_GREY_2)
// TODO(crbug.com/1015113): The EG2 macro is breaking indexing for some reason
// without the trailing semicolon.  For now, disable the extra semi warning
// so Xcode indexing works for the egtest.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(SettingsAppInterface);
#endif  // defined(CHROME_EARL_GREY_2)

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::ClearBrowsingDataButton;
using chrome_test_util::ClearBrowsingDataView;
using chrome_test_util::ClearBrowsingHistoryButton;
using chrome_test_util::ClearCacheButton;
using chrome_test_util::ClearCookiesButton;
using chrome_test_util::ClearSavedPasswordsButton;
using chrome_test_util::SettingsCollectionView;
using chrome_test_util::SettingsDoneButton;
using chrome_test_util::SettingsMenuBackButton;
using chrome_test_util::SettingsMenuPrivacyButton;

namespace {

const char kUrl[] = "http://foo/browsing";
const char kUrlWithSetCookie[] = "http://foo/set_cookie";
const char kResponse[] = "bar";
const char kResponseWithSetCookie[] = "bar with set cookie";
NSString* const kCookieName = @"name";
NSString* const kCookieValue = @"value";

enum MetricsServiceType {
  kMetrics,
  kBreakpad,
  kBreakpadFirstLaunch,
};

// Matcher for the Clear Browsing Data cell on the Privacy screen.
id<GREYMatcher> ClearBrowsingDataCell() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
}

}  // namespace

// Settings tests for Chrome.
@interface SettingsTestCase : ChromeTestCase
@end

@implementation SettingsTestCase

- (void)tearDown {
  // It is possible for a test to fail with a menu visible, which can cause
  // future tests to fail.

  // Check if a sub-menu is still displayed. If so, close it.
  NSError* error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:SettingsMenuBackButton()]
        performAction:grey_tap()];
  }

  // Check if the Settings menu is displayed. If so, close it.
  error = nil;
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      assertWithMatcher:grey_notNil()
                  error:&error];
  if (!error) {
    [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
        performAction:grey_tap()];
  }

  [super tearDown];
}

// Performs the steps to clear browsing data. Must be called on the
// Clear Browsing Data settings screen, after having selected the data types
// scheduled for removal.
- (void)clearBrowsingData {
  [ChromeEarlGreyUI tapClearBrowsingDataMenuButton:ClearBrowsingDataButton()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          ConfirmClearBrowsingDataButton()]
      performAction:grey_tap()];

  // Before returning, make sure that the top of the Clear Browsing Data
  // settings screen is visible to match the state at the start of the method.
  // TODO(crbug.com/973708): On iOS 13 the settings menu appears as a card that
  // can be dismissed with a downward swipe.  This make it difficult to use a
  // gesture to return to the top of the Clear Browsing Data screen, so scroll
  // programatically instead. Remove this custom action if we switch back to a
  // fullscreen presentation.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingDataView()]
      performAction:[ChromeActionsAppInterface scrollToTop]];
}

// From the NTP, clears the cookies and site data via the UI.
- (void)clearCookiesAndSiteData {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];

  // "Browsing history", "Cookies, Site Data" and "Cached Images and Files"
  // are the default checked options when the prefs are registered. Uncheck
  // "Browsing history" and "Cached Images and Files".
  // Prefs are reset to default at the end of each test.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];

  [self clearBrowsingData];
  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// From the NTP, clears the saved passwords via the UI.
- (void)clearPasswords {
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:SettingsMenuPrivacyButton()];
  [ChromeEarlGreyUI tapPrivacyMenuButton:ClearBrowsingDataCell()];

  // "Browsing history", "Cookies, Site Data" and "Cached Images and Files"
  // are the default checked options when the prefs are registered. Unckeck all
  // of them and check "Passwords".
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];

  [self clearBrowsingData];

  // Re-tap all the previously tapped cells, so that the default state of the
  // checkmarks is preserved.
  [[EarlGrey selectElementWithMatcher:ClearBrowsingHistoryButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCookiesButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearCacheButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:ClearSavedPasswordsButton()]
      performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:SettingsDoneButton()]
      performAction:grey_tap()];
}

// Checks for a given service that it is both recording and uploading, where
// appropriate.
- (void)assertMetricsServiceEnabled:(MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics:
      GREYAssertTrue([SettingsAppInterface isMetricsRecordingEnabled],
                     @"Metrics recording should be enabled.");
      GREYAssertTrue([SettingsAppInterface isMetricsReportingEnabled],
                     @"Metrics reporting should be enabled.");
      break;
    case kBreakpad:
      GREYAssertTrue([SettingsAppInterface isBreakpadEnabled],
                     @"Breakpad should be enabled.");
      GREYAssertTrue([SettingsAppInterface isBreakpadReportingEnabled],
                     @"Breakpad reporting should be enabled.");
      break;
    case kBreakpadFirstLaunch:
      // For first launch after upgrade, or after install, uploading of crash
      // reports is always disabled.  Check that the first launch flag is being
      // honored.
      GREYAssertTrue([SettingsAppInterface isBreakpadEnabled],
                     @"Breakpad should be enabled.");
      GREYAssertFalse([SettingsAppInterface isBreakpadReportingEnabled],
                      @"Breakpad reporting should be disabled.");
      break;
  }
}

// Checks for a given service that it is completely disabled.
- (void)assertMetricsServiceDisabled:(MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics: {
      GREYAssertFalse([SettingsAppInterface isMetricsRecordingEnabled],
                      @"Metrics recording should be disabled.");
      GREYAssertFalse([SettingsAppInterface isMetricsReportingEnabled],
                      @"Metrics reporting should be disabled.");
      break;
    }
    case kBreakpad:
    case kBreakpadFirstLaunch: {
      // Check only whether or not breakpad is enabled.  Disabling
      // breakpad does stop uploading, and does not change the flag
      // used to check whether or not it's uploading.
      GREYAssertFalse([SettingsAppInterface isBreakpadEnabled],
                      @"Breakpad should be disabled.");
      break;
    }
  }
}

// Checks for a given service that it is recording, but not uploading anything.
// Used to test that the wifi-only preference is honored when the device is
// using a cellular network.
- (void)assertMetricsServiceEnabledButNotUploading:
    (MetricsServiceType)serviceType {
  switch (serviceType) {
    case kMetrics: {
      GREYAssertTrue([SettingsAppInterface isMetricsRecordingEnabled],
                     @"Metrics recording should be enabled.");
      GREYAssertFalse([SettingsAppInterface isMetricsReportingEnabled],
                      @"Metrics reporting should be disabled.");
      break;
    }
    case kBreakpad:
    case kBreakpadFirstLaunch: {
      GREYAssertTrue([SettingsAppInterface isBreakpadEnabled],
                     @"Breakpad should be enabled.");
      GREYAssertFalse([SettingsAppInterface isBreakpadReportingEnabled],
                      @"Breakpad reporting should be disabled.");
      break;
    }
  }
}

- (void)assertsMetricsPrefsForService:(MetricsServiceType)serviceType {
  // Two preferences, each with two values - on or off.  Check all four
  // combinations:
  // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly OFF
  //  - Services do not record data and do not upload data.

  // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly ON
  //  - Services do not record data and do not upload data.
  //    Note that if kMetricsReportingEnabled is OFF, the state of
  //    kMetricsReportingWifiOnly does not matter.

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON
  //  - Services record data and upload data only when the device is using
  //    a wifi connection.  Note:  rather than checking for wifi, the code
  //    checks for a cellular network (wwan).  wwan != wifi.  So if wwan is
  //    true, services do not upload any data.

  // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
  //  - Services record data and upload data.

  if ([ChromeEarlGrey isUMACellularEnabled]) {
    // kMetricsReportingEnabled OFF
    [SettingsAppInterface setMetricsReportingEnabled:NO];
  } else {
    // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly OFF
    [SettingsAppInterface setMetricsReportingEnabled:NO wifiOnly:NO];
  }
  // Service should be completely disabled.
  // I.e. no recording of data, and no uploading of what's been recorded.
  [self assertMetricsServiceDisabled:serviceType];

  if ([ChromeEarlGrey isUMACellularEnabled]) {
    // kMetricsReportingEnabled OFF
    [SettingsAppInterface setMetricsReportingEnabled:NO];
  } else {
    // kMetricsReportingEnabled OFF and kMetricsReportingWifiOnly ON
    [SettingsAppInterface setMetricsReportingEnabled:NO wifiOnly:YES];
  }
  // If kMetricsReportingEnabled is OFF, any service should remain completely
  // disabled, i.e. no uploading even if kMetricsReportingWifiOnly is ON.
  [self assertMetricsServiceDisabled:serviceType];

// Split here:  Official build vs. Development build.
// Official builds allow recording and uploading of data, honoring the
// metrics prefs.  Development builds should never record or upload data.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Official build.
  // The values of the prefs and the wwan vs wifi state should be honored by
  // the services, turning on and off according to the rules laid out above.

  if (![ChromeEarlGrey isUMACellularEnabled]) {
    // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON.
    [SettingsAppInterface setMetricsReportingEnabled:YES wifiOnly:YES];
    // Service should be enabled.
    [self assertMetricsServiceEnabled:serviceType];

    // Set the network to use a cellular network, which should disable uploading
    // when the wifi-only flag is set.
    [SettingsAppInterface setCellularNetworkEnabled:YES];
    [self assertMetricsServiceEnabledButNotUploading:serviceType];

    // Turn off cellular network usage, which should enable uploading.
    [SettingsAppInterface setCellularNetworkEnabled:NO];
    [self assertMetricsServiceEnabled:serviceType];

    // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
    [SettingsAppInterface setMetricsReportingEnabled:YES wifiOnly:NO];
    [self assertMetricsServiceEnabled:serviceType];
  }

#else
  // Development build.  Do not allow any recording or uploading of data.
  // Specifically, the kMetricsReportingEnabled preference is completely
  // disregarded for non-official builds, and checking its value always returns
  // false (NO).
  // This tests that no matter the state change, pref or network connection,
  // services remain disabled.

  if (![ChromeEarlGrey isUMACellularEnabled]) {
    // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly ON
    [SettingsAppInterface setMetricsReportingEnabled:YES wifiOnly:YES];
    // Service should remain disabled.
    [self assertMetricsServiceDisabled:serviceType];

    // kMetricsReportingEnabled ON and kMetricsReportingWifiOnly OFF
    [SettingsAppInterface setMetricsReportingEnabled:YES wifiOnly:NO];
    // Service should remain disabled.
    [self assertMetricsServiceDisabled:serviceType];
  }
#endif
}

#pragma mark Tests

// Tests that clearing the cookies through the UI does clear all of them. Use a
// local server to navigate to a page that sets then tests a cookie, and then
// clears the cookie and tests it is not set.
- (void)testClearCookies {
  // Creates a map of canned responses and set up the test HTML server.
  std::map<GURL, std::pair<std::string, std::string>> response;

  NSString* cookieForURL =
      [NSString stringWithFormat:@"%@=%@", kCookieName, kCookieValue];

  response[web::test::HttpServer::MakeUrl(kUrlWithSetCookie)] =
      std::pair<std::string, std::string>(base::SysNSStringToUTF8(cookieForURL),
                                          kResponseWithSetCookie);
  response[web::test::HttpServer::MakeUrl(kUrl)] =
      std::pair<std::string, std::string>("", kResponse);

  web::test::SetUpSimpleHttpServerWithSetCookies(response);

  // Load |kUrl| and check that cookie is not set.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse];

  NSDictionary* cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count, @"No cookie should be found.");

  // Visit |kUrlWithSetCookie| to set a cookie and then load |kUrl| to check it
  // is still set.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrlWithSetCookie)];
  [ChromeEarlGrey waitForWebStateContainingText:kResponseWithSetCookie];
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse];

  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqualObjects(kCookieValue, cookies[kCookieName],
                         @"Failed to set cookie.");
  GREYAssertEqual(1U, cookies.count, @"Only one cookie should be found.");

  // Restore the Clear Browsing Data checkmarks prefs to their default state
  // in Teardown.
  [self setTearDownHandler:^{
    [SettingsAppInterface restoreClearBrowsingDataCheckmarksToDefault];
  }];

  // Clear all cookies.
  [self clearCookiesAndSiteData];

  // Reload and test that there are no cookies left.
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kUrl)];
  [ChromeEarlGrey waitForWebStateContainingText:kResponse];

  cookies = [ChromeEarlGrey cookies];
  GREYAssertEqual(0U, cookies.count, @"No cookie should be found.");

  [ChromeEarlGrey closeAllTabs];
}

// Verifies that metrics reporting works properly under possible settings of the
// preferences kMetricsReportingEnabled and kMetricsReportingWifiOnly.
- (void)testMetricsReporting {
  [self assertsMetricsPrefsForService:kMetrics];
}

// Verifies that breakpad reporting works properly under possible settings of
// the preferences |kMetricsReportingEnabled| and |kMetricsReportingWifiOnly|
// for non-first-launch runs.
// NOTE: breakpad only allows uploading for non-first-launch runs.
- (void)testBreakpadReporting {
  [self setTearDownHandler:^{
    // Restore the first launch state to previous state.
    [SettingsAppInterface resetFirstLaunchState];
  }];

  [SettingsAppInterface setFirstLunchState:NO];
  [self assertsMetricsPrefsForService:kBreakpad];
}

// Verifies that breakpad reporting works properly under possible settings of
// the preferences |kMetricsReportingEnabled| and |kMetricsReportingWifiOnly|
// for first-launch runs.
// NOTE: breakpad only allows uploading for non-first-launch runs.
- (void)testBreakpadReportingFirstLaunch {
  [self setTearDownHandler:^{
    // Restore the first launch state to previous state.
    [SettingsAppInterface resetFirstLaunchState];
  }];

  [SettingsAppInterface setFirstLunchState:YES];
  [self assertsMetricsPrefsForService:kBreakpadFirstLaunch];
}

// Verifies that the Settings UI registers keyboard commands when presented, but
// not when it itslef presents something.
- (void)testSettingsKeyboardCommands {
  [ChromeEarlGreyUI openSettingsMenu];
  [[EarlGrey selectElementWithMatcher:SettingsCollectionView()]
      assertWithMatcher:grey_notNil()];

  // Verify that the Settings register keyboard commands.
  GREYAssertTrue([SettingsAppInterface settingsRegisteredKeyboardCommands],
                 @"Settings should register key commands when presented.");

  // Present the Sign-in UI.
  id<GREYMatcher> matcher = grey_allOf(chrome_test_util::PrimarySignInButton(),
                                       grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
  // Wait for UI to finish loading the Sign-in screen.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Verify that the Settings register keyboard commands.
  GREYAssertFalse([SettingsAppInterface settingsRegisteredKeyboardCommands],
                  @"Settings should not register key commands when presented.");

  // Cancel the sign-in operation.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kSkipSigninAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Wait for UI to finish closing the Sign-in screen.
  [[GREYUIThreadExecutor sharedInstance] drainUntilIdle];

  // Verify that the Settings register keyboard commands.
  GREYAssertTrue([SettingsAppInterface settingsRegisteredKeyboardCommands],
                 @"Settings should register key commands when presented.");
}

@end
