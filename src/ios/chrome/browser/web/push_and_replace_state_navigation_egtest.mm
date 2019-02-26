// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/test/url_test_util.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::BackButton;
using chrome_test_util::ForwardButton;

namespace {

const char* kHistoryTestUrl =
    "http://ios/testing/data/http_server_files/history.html";
const char* kNonPushedUrl =
    "http://ios/testing/data/http_server_files/pony.html";
const char* kReplaceStateHashWithObjectURL =
    "http://ios/testing/data/http_server_files/history.html#replaceWithObject";
const char* kPushStateHashStringURL =
    "http://ios/testing/data/http_server_files/history.html#string";
const char* kReplaceStateHashStringURL =
    "http://ios/testing/data/http_server_files/history.html#replace";
const char* kPushStatePathURL =
    "http://ios/testing/data/http_server_files/path";
const char* kReplaceStateRootPathSpaceURL = "http://ios/rep lace";

}  // namespace

// Tests for pushState/replaceState navigations.
@interface PushAndReplaceStateNavigationTestCase : ChromeTestCase
@end

@implementation PushAndReplaceStateNavigationTestCase

// Tests calling history.pushState() multiple times and navigating back/forward.
- (void)testHtml5HistoryPushStateThenGoBackAndForward {
  const GURL pushStateHashWithObjectURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/history.html#pushWithObject");
  const std::string pushStateHashWithObjectOmniboxText =
      web::GetContentAndFragmentForUrl(pushStateHashWithObjectURL);
  const GURL pushStateRootPathURL =
      web::test::HttpServer::MakeUrl("http://ios/rootpath");
  const std::string pushStateRootPathOmniboxText =
      web::GetContentAndFragmentForUrl(pushStateRootPathURL);
  const GURL pushStatePathSpaceURL =
      web::test::HttpServer::MakeUrl("http://ios/pa%20th");
  const std::string pushStatePathSpaceOmniboxText =
      web::GetContentAndFragmentForUrl(pushStatePathSpaceURL);
  web::test::SetUpFileBasedHttpServer();
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kHistoryTestUrl)];

  // Push 3 URLs. Verify that the URL changed and the status was updated.
  [ChromeEarlGrey tapWebViewElementWithID:@"pushStateHashWithObject"];
  [self assertStatusText:@"pushStateHashWithObject"
         withOmniboxText:pushStateHashWithObjectOmniboxText
              pageLoaded:NO];

  [ChromeEarlGrey tapWebViewElementWithID:@"pushStateRootPath"];
  [self assertStatusText:@"pushStateRootPath"
         withOmniboxText:pushStateRootPathOmniboxText
              pageLoaded:NO];

  [ChromeEarlGrey tapWebViewElementWithID:@"pushStatePathSpace"];
  [self assertStatusText:@"pushStatePathSpace"
         withOmniboxText:pushStatePathSpaceOmniboxText
              pageLoaded:NO];

  // Go back and check that the page doesn't load and the status text is updated
  // by the popstate event.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"pushStateRootPath"
         withOmniboxText:pushStateRootPathOmniboxText
              pageLoaded:NO];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"pushStateHashWithObject"
         withOmniboxText:pushStateHashWithObjectOmniboxText
              pageLoaded:NO];

  [ChromeEarlGrey tapWebViewElementWithID:@"goBack"];
  const GURL historyTestURL = web::test::HttpServer::MakeUrl(kHistoryTestUrl);
  [self assertStatusText:nil
         withOmniboxText:web::GetContentAndFragmentForUrl(historyTestURL)
              pageLoaded:NO];

  // Go forward 2 pages and check that the page doesn't load and the status text
  // is updated by the popstate event.
  [ChromeEarlGrey tapWebViewElementWithID:@"goForward2"];
  [self assertStatusText:@"pushStateRootPath"
         withOmniboxText:pushStateRootPathOmniboxText
              pageLoaded:NO];
}

// Tests that calling replaceState() changes the current history entry.
- (void)testHtml5HistoryReplaceStateThenGoBackAndForward {
  web::test::SetUpFileBasedHttpServer();
  const GURL initialURL = web::test::HttpServer::MakeUrl(kNonPushedUrl);
  const std::string initialOmniboxText =
      web::GetContentAndFragmentForUrl(initialURL);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kHistoryTestUrl)];

  // Replace the URL and go back then forward.
  const GURL replaceStateHashWithObjectURL =
      web::test::HttpServer::MakeUrl(kReplaceStateHashWithObjectURL);
  const std::string replaceStateHashWithObjectOmniboxText =
      web::GetContentAndFragmentForUrl(replaceStateHashWithObjectURL);
  [ChromeEarlGrey tapWebViewElementWithID:@"replaceStateHashWithObject"];
  [self assertStatusText:@"replaceStateHashWithObject"
         withOmniboxText:replaceStateHashWithObjectOmniboxText
              pageLoaded:NO];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          initialOmniboxText)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:ForwardButton()]
      performAction:grey_tap()];
  // TODO(crbug.com/776606): WKWebView doesn't fire load event for back/forward
  // navigation and WKBasedNavigationManager inherits this behavior.
  bool expectOnLoad = !web::GetWebClient()->IsSlimNavigationManagerEnabled();
  [self assertStatusText:@"replaceStateHashWithObject"
         withOmniboxText:replaceStateHashWithObjectOmniboxText
              pageLoaded:expectOnLoad];

  // Push URL then replace it. Do this twice.
  const GURL pushStateHashStringURL =
      web::test::HttpServer::MakeUrl(kPushStateHashStringURL);
  const std::string pushStateHashStringOmniboxText =
      web::GetContentAndFragmentForUrl(pushStateHashStringURL);
  [ChromeEarlGrey tapWebViewElementWithID:@"pushStateHashString"];
  [self assertStatusText:@"pushStateHashString"
         withOmniboxText:pushStateHashStringOmniboxText
              pageLoaded:NO];

  const GURL replaceStateHashStringURL =
      web::test::HttpServer::MakeUrl(kReplaceStateHashStringURL);
  const std::string replaceStateHashStringOmniboxText =
      web::GetContentAndFragmentForUrl(replaceStateHashStringURL);
  [ChromeEarlGrey tapWebViewElementWithID:@"replaceStateHashString"];
  [self assertStatusText:@"replaceStateHashString"
         withOmniboxText:replaceStateHashStringOmniboxText
              pageLoaded:NO];

  const GURL pushStatePathURL =
      web::test::HttpServer::MakeUrl(kPushStatePathURL);
  const std::string pushStatePathOmniboxText =
      web::GetContentAndFragmentForUrl(pushStatePathURL);
  [ChromeEarlGrey tapWebViewElementWithID:@"pushStatePath"];
  [self assertStatusText:@"pushStatePath"
         withOmniboxText:pushStatePathOmniboxText
              pageLoaded:NO];

  const GURL replaceStateRootPathSpaceURL =
      web::test::HttpServer::MakeUrl(kReplaceStateRootPathSpaceURL);
  const std::string replaceStateRootPathSpaceOmniboxText =
      web::GetContentAndFragmentForUrl(replaceStateRootPathSpaceURL);
  [ChromeEarlGrey tapWebViewElementWithID:@"replaceStateRootPathSpace"];
  [self assertStatusText:@"replaceStateRootPathSpace"
         withOmniboxText:replaceStateRootPathSpaceOmniboxText
              pageLoaded:NO];

  // Go back and check URLs.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"replaceStateHashString"
         withOmniboxText:replaceStateHashStringOmniboxText
              pageLoaded:NO];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"replaceStateHashWithObject"
         withOmniboxText:replaceStateHashWithObjectOmniboxText
              pageLoaded:NO];

  // Go forward and check URL.
  [ChromeEarlGrey tapWebViewElementWithID:@"goForward2"];
  [self assertStatusText:@"replaceStateRootPathSpace"
         withOmniboxText:replaceStateRootPathSpaceOmniboxText
              pageLoaded:NO];
}

// Tests that page loads occur when navigating to or past a non-pushed URL.
- (void)testHtml5HistoryNavigatingPastNonPushedURL {
  GURL nonPushedURL = web::test::HttpServer::MakeUrl(kNonPushedUrl);
  web::test::SetUpFileBasedHttpServer();
  const GURL historyTestURL = web::test::HttpServer::MakeUrl(kHistoryTestUrl);
  [ChromeEarlGrey loadURL:historyTestURL];
  const std::string historyTestOmniboxText =
      web::GetContentAndFragmentForUrl(historyTestURL);

  // Push same URL twice. Verify that URL changed and the status was updated.
  const GURL pushStateHashStringURL =
      web::test::HttpServer::MakeUrl(kPushStateHashStringURL);
  const std::string pushStateHashStringOmniboxText =
      web::GetContentAndFragmentForUrl(pushStateHashStringURL);
  [ChromeEarlGrey tapWebViewElementWithID:@"pushStateHashString"];
  [self assertStatusText:@"pushStateHashString"
         withOmniboxText:pushStateHashStringOmniboxText
              pageLoaded:NO];
  [ChromeEarlGrey tapWebViewElementWithID:@"pushStateHashString"];
  [self assertStatusText:@"pushStateHashString"
         withOmniboxText:pushStateHashStringOmniboxText
              pageLoaded:NO];

  // Load a non-pushed URL.
  [ChromeEarlGrey loadURL:nonPushedURL];

  // Load history.html and push another URL.
  [ChromeEarlGrey loadURL:historyTestURL];
  [ChromeEarlGrey tapWebViewElementWithID:@"pushStateHashString"];
  [self assertStatusText:@"pushStateHashString"
         withOmniboxText:pushStateHashStringOmniboxText
              pageLoaded:NO];

  // At this point the history looks like this:
  // [NTP, history.html, #string, #string, nonPushedURL, history.html, #string]

  // Go back (to second history.html) and verify page did not load.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:nil
         withOmniboxText:historyTestOmniboxText
              pageLoaded:NO];

  // Go back twice (to second #string) and verify page did load.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  // TODO(crbug.com/776606): WKWebView doesn't fire load event for back/forward
  // navigation and WKBasedNavigationManager inherits this behavior.
  bool expectOnLoad = !web::GetWebClient()->IsSlimNavigationManagerEnabled();
  [self assertStatusText:nil
         withOmniboxText:pushStateHashStringOmniboxText
              pageLoaded:expectOnLoad];

  // Go back once (to first #string) and verify page did not load.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:@"pushStateHashString"
         withOmniboxText:pushStateHashStringOmniboxText
              pageLoaded:NO];

  // Go forward 4 entries at once (to third #string) and verify page did load.
  [ChromeEarlGrey tapWebViewElementWithID:@"goForward4"];

  [self assertStatusText:nil
         withOmniboxText:pushStateHashStringOmniboxText
              pageLoaded:expectOnLoad];

  // Go back 4 entries at once (to first #string) and verify page did load.
  [ChromeEarlGrey tapWebViewElementWithID:@"goBack4"];

  [self assertStatusText:nil
         withOmniboxText:pushStateHashStringOmniboxText
              pageLoaded:expectOnLoad];
}

// Tests calling pushState with unicode characters.
- (void)testHtml5HistoryPushUnicodeCharacters {
  // The GURL object %-escapes Unicode characters in the URL's fragment,
  // but the omnibox decodes them back to Unicode for display.
  std::string pushStateUnicode =
      web::GetContentAndFragmentForUrl(web::test::HttpServer::MakeUrl(
          "http://ios/testing/data/http_server_files/"
          "history.html#unicode")) +
      "\xe1\x84\x91";
  std::string pushStateUnicode2 =
      web::GetContentAndFragmentForUrl(web::test::HttpServer::MakeUrl(
          "http://ios/testing/data/http_server_files/"
          "history.html#unicode2")) +
      "\xe2\x88\xa2";
  const char pushStateUnicodeLabel[] = "Action: pushStateUnicodeᄑ";
  NSString* pushStateUnicodeStatus = @"pushStateUnicodeᄑ";
  const char pushStateUnicode2Label[] = "Action: pushStateUnicode2∢";
  NSString* pushStateUnicode2Status = @"pushStateUnicode2∢";

  web::test::SetUpFileBasedHttpServer();
  [ChromeEarlGrey loadURL:web::test::HttpServer::MakeUrl(kHistoryTestUrl)];

  // Do 2 push states with unicode characters.
  [ChromeEarlGrey tapWebViewElementWithID:@"pushStateUnicode"];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(pushStateUnicode)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebViewContainingText:pushStateUnicodeLabel];

  [ChromeEarlGrey tapWebViewElementWithID:@"pushStateUnicode2"];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(pushStateUnicode2)]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebViewContainingText:pushStateUnicode2Label];

  // Do a push state without a unicode character.
  const GURL pushStatePathURL =
      web::test::HttpServer::MakeUrl(kPushStatePathURL);
  const std::string pushStatePathOmniboxText =
      web::GetContentAndFragmentForUrl(pushStatePathURL);
  [ChromeEarlGrey tapWebViewElementWithID:@"pushStatePath"];

  [self assertStatusText:@"pushStatePath"
         withOmniboxText:pushStatePathOmniboxText
              pageLoaded:NO];

  // Go back and check the unicode in the URL and status.
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:pushStateUnicode2Status
         withOmniboxText:pushStateUnicode2
              pageLoaded:NO];

  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
  [self assertStatusText:pushStateUnicodeStatus
         withOmniboxText:pushStateUnicode
              pageLoaded:NO];
}

// Tests that pushState/replaceState handling properly handles <base>.
- (void)testHtml5HistoryWithBase {
  std::map<GURL, std::string> responses;
  GURL originURL =
      web::test::HttpServer::MakeUrl("http://foo.com/foo/bar.html");
  GURL pushResultURL = originURL.GetOrigin().Resolve("pushed/relative/url");
  const std::string pushResultOmniboxText =
      web::GetContentAndFragmentForUrl(pushResultURL);
  GURL replaceResultURL =
      originURL.GetOrigin().Resolve("replaced/relative/url");
  const std::string replaceResultOmniboxText =
      web::GetContentAndFragmentForUrl(replaceResultURL);

  // A simple HTML page with a base tag that makes all relative URLs
  // domain-relative, a button to trigger a relative pushState, and a button
  // to trigger a relative replaceState.
  NSString* baseTag = @"<base href=\"/\">";
  NSString* pushAndReplaceButtons =
      @"<input type=\"button\" value=\"pushState\" "
       "id=\"pushState\" onclick=\"history.pushState("
       "{}, 'Foo', './pushed/relative/url');\"><br>"
       "<input type=\"button\" value=\"replaceState\" "
       "id=\"replaceState\" onclick=\"history.replaceState("
       "{}, 'Foo', './replaced/relative/url');\"><br>";
  NSString* simplePage =
      @"<!doctype html><html><head>%@</head><body>%@</body></html>";
  responses[originURL] = base::SysNSStringToUTF8(
      [NSString stringWithFormat:simplePage, baseTag, pushAndReplaceButtons]);
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:originURL];
  [ChromeEarlGrey tapWebViewElementWithID:@"pushState"];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pushResultOmniboxText)]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey tapWebViewElementWithID:@"replaceState"];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          replaceResultOmniboxText)]
      assertWithMatcher:grey_notNil()];
}

#pragma mark - Utility methods

// Assert that status text |status|, if non-nil, is displayed in the webview,
// that the omnibox text is as expected, and that "onload" text is displayed if
// pageLoaded is YES.
- (void)assertStatusText:(NSString*)status
         withOmniboxText:(const std::string&)omniboxText
              pageLoaded:(BOOL)pageLoaded {
  if (pageLoaded) {
    [ChromeEarlGrey waitForWebViewContainingText:"onload"];
  } else {
    [ChromeEarlGrey waitForWebViewNotContainingText:"onload"];
  }

  if (status != nil) {
    NSString* statusLabel = [NSString stringWithFormat:@"Action: %@", status];
    [ChromeEarlGrey
        waitForWebViewContainingText:base::SysNSStringToUTF8(statusLabel)];
  }

  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OmniboxText(omniboxText)]
      assertWithMatcher:grey_notNil()];
}

@end
