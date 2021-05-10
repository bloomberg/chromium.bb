// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <string>

#include "base/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ios/testing/embedded_test_server_handlers.h"
#include "ios/web/common/features.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns ERR_CONNECTION_CLOSED error message.
std::string GetErrorMessage() {
  return net::ErrorToShortString(net::ERR_CONNECTION_CLOSED);
}
}  // namespace

// Tests critical user journeys reloated to page load errors.
@interface ErrorPageTestCase : ChromeTestCase
// YES if test server is replying with valid HTML content (URL query). NO if
// test server closes the socket.
@property(atomic) bool serverRespondsWithContent;
@end

@implementation ErrorPageTestCase
@synthesize serverRespondsWithContent = _serverRespondsWithContent;

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;

  // Features are enabled or disabled based on the name of the test that is
  // running. This is done because it is inefficient to use
  // ensureAppLaunchedWithConfiguration for each test.
  if ([self isRunningTest:@selector(testRestoreErrorPage)]) {
    config.features_disabled.push_back(kEnableCloseAllTabsConfirmation);
    config.features_enabled.push_back(web::features::kUseJSForErrorPage);
  } else {
    config.features_enabled.push_back(kEnableCloseAllTabsConfirmation);
  }
  return config;
}

- (void)setUp {
  [super setUp];

  RegisterDefaultHandlers(self.testServer);
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/echo-query",
      base::BindRepeating(&testing::HandleEchoQueryOrCloseSocket,
                          std::cref(_serverRespondsWithContent))));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that the error page is correctly displayed after navigating back to it
// multiple times. See http://crbug.com/944037 .
- (void)testBackForwardErrorPage {
  // TODO(crbug.com/1153261): Going back/forward on the same host is failing.
  // Use chrome:// to have a different hosts.
  std::string errorText = net::ErrorToShortString(net::ERR_INVALID_URL);
  self.serverRespondsWithContent = YES;

  [ChromeEarlGrey loadURL:GURL("chrome://invalid")];
  [ChromeEarlGrey waitForWebStateContainingText:errorText];
  // Add some delay otherwise the back/forward navigations are occurring too
  // fast.
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.2));

  // Navigate to a page which responds.
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?bar")];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.2));

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:errorText];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.2));

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.2));

  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:errorText];
  base::test::ios::SpinRunLoopWithMinDelay(base::TimeDelta::FromSecondsD(0.2));

  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];
}

// Loads the URL which fails to load, then sucessfully navigates back/forward to
// the page.
- (void)testNavigateForwardToErrorPage {
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?bar")];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];

  // No response leads to ERR_CONNECTION_CLOSED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];

  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey goBack];
  [ChromeEarlGrey waitForWebStateContainingText:"bar"];

  // Navigate forward to the error page, which should load without errors.
  [ChromeEarlGrey goForward];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];
}

// Loads the URL which fails to load, then sucessfully reloads the page.
- (void)testReloadErrorPage {
  // No response leads to ERR_CONNECTION_CLOSED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];

  // Reload the page, which should load without errors.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];
}

// Sucessfully loads the page, stops the server and reloads the page.
- (void)testReloadPageAfterServerIsDown {
  // Sucessfully load the page.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];

  // Reload the page, no response leads to ERR_CONNECTION_CLOSED error.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey reload];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];
}

// Loads a URL then restore the session and fail during the reload
- (void)testRestoreErrorPage {
  // Load the page.
  self.serverRespondsWithContent = YES;
  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo-query?foo")];
  [ChromeEarlGrey waitForWebStateContainingText:"foo"];
  GREYAssertEqual(1, [ChromeEarlGrey navigationBackListItemsCount],
                  @"The navigation back list should have only 1 entries before "
                  @"the restoration.");

  // Restore the session but with the page no longer loading.
  self.serverRespondsWithContent = NO;
  [ChromeEarlGrey triggerRestoreViaTabGridRemoveAllUndo];
  [ChromeEarlGrey waitForWebStateContainingText:GetErrorMessage()];

  GREYAssertEqual(1, [ChromeEarlGrey navigationBackListItemsCount],
                  @"The navigation back list should still have only 1 entries "
                  @"after the restoration.");
}

@end
