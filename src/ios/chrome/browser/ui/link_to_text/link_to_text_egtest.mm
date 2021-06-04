// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <vector>

#import "base/ios/ios_util.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "components/shared_highlighting/core/common/text_fragments_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_actions_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#import "ios/web/public/test/element_selector.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "net/test/embedded_test_server/request_handler_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using shared_highlighting::TextFragment;

namespace {

const char kFirstFragmentText[] = "Hello foo!";
const char kSecondFragmentText[] = "bar";
const char kTestPageTextSample[] = "Lorem ipsum";
const char kNoTextTestPageTextSample[] = "only boundary";
const char kInputTestPageTextSample[] = "has an input";
const char kSimpleTextElementId[] = "toBeSelected";
const char kToBeSelectedText[] = "VeryUniqueWord";

const char kTestURL[] = "/testPage";
const char kURLWithTwoFragments[] = "/testPage/#:~:text=Hello%20foo!&text=bar";
const char kHTMLOfTestPage[] =
    "<html><body>"
    "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Hello foo! Ut enim ad "
    "minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
    "ex ea "
    "commodo consequat. bar</p>"
    "<p id=\"toBeSelected\">VeryUniqueWord</p>"
    "</body></html>";

const char kTestLongPageURL[] = "/longTestPage";
const char kLongPageURLWithOneFragment[] =
    "/longTestPage/#:~:text=Hello%20foo!";
const char kHTMLOfLongTestPage[] =
    "<html><body>"
    "<div style=\"background:blue; height: 4000px; width: 250px;\"></div>"
    "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Hello foo! Ut enim ad "
    "minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip "
    "ex ea "
    "commodo consequat. bar</p>"
    "<div style=\"background:blue; height: 4000px; width: 250px;\"></div>"
    "</body></html>";

const char kNoTextTestURL[] = "/noTextPage";
const char kHTMLOfNoTextTestPage[] =
    "<html><body>"
    "This page has a paragraph with only boundary characters"
    "<p id=\"toBeSelected\"> .!, \t</p>"
    "</body></html>";

const char kInputTestURL[] = "/inputTextPage";
const char kHTMLOfInputTestPage[] =
    "<html><body>"
    "This page has an input"
    "<input type=\"text\" id=\"toBeSelected\"></p>"
    "</body></html>";

NSArray<NSString*>* GetMarkedText() {
  NSString* js = @"(function() {"
                  "  const marks = document.getElementsByTagName('mark');"
                  "  const markedText = [];"
                  "  for (const mark of marks) {"
                  "    if (mark && mark.innerText) {"
                  "      markedText.push(mark.innerText);"
                  "    }"
                  "  }"
                  "  return markedText;"
                  "})();";
  return [ChromeEarlGrey executeJavaScript:js];
}

NSString* GetFirstVisibleMarkedText() {
  NSString* js =
      @"(function () {"
       "  const firstMark = document.getElementsByTagName('mark')[0];"
       "  if (!firstMark) {"
       "    return '';"
       "  }"
       "  const rect = firstMark.getBoundingClientRect();"
       "  const isVisible = rect.top >= 0 &&"
       "    rect.bottom <= window.innerHeight &&"
       "    rect.left >= 0 &&"
       "    rect.right <= window.innerWidth;"
       "  return isVisible ? firstMark.innerText : '';"
       "})();";
  return [ChromeEarlGrey executeJavaScript:js];
}

std::unique_ptr<net::test_server::HttpResponse> LoadHtml(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_content_type("text/html");
  http_response->set_content(html);
  return std::move(http_response);
}

}  // namespace

// Test class for the scroll-to-text and link-to-text features.
@interface LinkToTextTestCase : ChromeTestCase
@end

@implementation LinkToTextTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kSharedHighlightingIOS);
  return config;
}

- (void)setUp {
  [super setUp];

  RegisterDefaultHandlers(self.testServer);
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&net::test_server::HandlePrefixedRequest, kTestURL,
                          base::BindRepeating(&LoadHtml, kHTMLOfTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kTestLongPageURL,
      base::BindRepeating(&LoadHtml, kHTMLOfLongTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kNoTextTestURL,
      base::BindRepeating(&LoadHtml, kHTMLOfNoTextTestPage)));
  self.testServer->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, kInputTestURL,
      base::BindRepeating(&LoadHtml, kHTMLOfInputTestPage)));

  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that navigating to a URL with text fragments will highlight all
// fragments.
- (void)testHighlightAllFragments {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kURLWithTwoFragments)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  NSArray<NSString*>* markedText = GetMarkedText();

  GREYAssertEqual(2, markedText.count,
                  @"Did not get the expected number of marked text.");
  GREYAssertEqual(kFirstFragmentText, base::SysNSStringToUTF8(markedText[0]),
                  @"First marked text is not valid.");
  GREYAssertEqual(kSecondFragmentText, base::SysNSStringToUTF8(markedText[1]),
                  @"Second marked text is not valid.");
}

// Tests that a fragment will be scrolled to if it's lower on the page.
- (void)testScrollToHighlight {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kLongPageURLWithOneFragment)];
  [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

  __block NSString* firstVisibleMark;
  GREYCondition* scrolledToText = [GREYCondition
      conditionWithName:@"Did not scroll to marked text."
                  block:^{
                    NSString* visibleMarkedText = GetFirstVisibleMarkedText();
                    if (visibleMarkedText &&
                        visibleMarkedText != [NSString string]) {
                      firstVisibleMark = visibleMarkedText;
                      return YES;
                    }
                    return NO;
                  }];

  GREYAssert([scrolledToText
                 waitWithTimeout:base::test::ios::kWaitForJSCompletionTimeout],
             @"Could not find visible marked element.");

  GREYAssertEqual(kFirstFragmentText, base::SysNSStringToUTF8(firstVisibleMark),
                  @"Visible marked text is not valid.");
}

// Tests that a link can be generated for a simple text selection.
- (void)testGenerateLinkForSimpleText {
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    // Skip test on iOS 12 as the Activity View on that version is not
    // accessible by Earl Grey.
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iOS 12.");
  }

  // TODO(crbug.com/1149603): Re-enable this test on iPad once presenting
  // popovers work.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Test is disabled on iPad.");
  }

  if (@available(iOS 13, *)) {
    GURL pageURL = self.testServer->GetURL(kTestURL);
    [ChromeEarlGrey loadURL:pageURL];
    [ChromeEarlGrey waitForWebStateContainingText:kTestPageTextSample];

    [ChromeTestCase removeAnyOpenMenusAndInfoBars];

    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:chrome_test_util::LongPressElementForContextMenu(
                          [ElementSelector
                              selectorWithElementID:kSimpleTextElementId],
                          true)];

    // Edit menu should be there.
    id<GREYMatcher> linkToTextButton =
        chrome_test_util::SystemSelectionCalloutLinkToTextButton();
    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:linkToTextButton];

    [[EarlGrey selectElementWithMatcher:linkToTextButton]
        performAction:grey_tap()];

    // Make sure the Edit menu is gone.
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::SystemSelectionCallout()]
        assertWithMatcher:grey_notVisible()];

    // Wait for the Activity View to show up (look for the Copy action).
    id<GREYMatcher> copyActivityButton = chrome_test_util::CopyActivityButton();
    [ChromeEarlGrey
        waitForSufficientlyVisibleElementWithMatcher:copyActivityButton];

    // Tap on the Copy action.
    [[EarlGrey selectElementWithMatcher:copyActivityButton]
        performAction:grey_tap()];

    // Assert the values stored in the pasteboard. Lower-casing the expected
    // GURL as that is what the JS library is doing.
    std::vector<TextFragment> fragments{
        TextFragment(base::ToLowerASCII(kToBeSelectedText))};
    GURL expectedGURL =
        shared_highlighting::AppendFragmentDirectives(pageURL, fragments);

    // Wait for the value to be in the pasteboard.
    GREYCondition* getPasteboardValue = [GREYCondition
        conditionWithName:@"Could not get an expected URL from the pasteboard."
                    block:^{
                      return expectedGURL == [ChromeEarlGrey pasteboardURL];
                    }];

    GREYAssert([getPasteboardValue
                   waitWithTimeout:base::test::ios::kWaitForActionTimeout],
               @"Could not get expected URL from pasteboard.");
  }
}

- (void)testBadSelectionDisablesGenerateLink {
  if (!base::ios::IsRunningOnIOS13OrLater()) {
    // The TextInput implementation is incomplete on iOS 13, so this condition
    // isn't enforced on older versions.
    EARL_GREY_TEST_SKIPPED(@"Test skipped on iOS 13.");
  }
  if (@available(iOS 14, *)) {
    [ChromeEarlGrey loadURL:self.testServer->GetURL(kNoTextTestURL)];
    [ChromeEarlGrey waitForWebStateContainingText:kNoTextTestPageTextSample];

    [ChromeTestCase removeAnyOpenMenusAndInfoBars];

    [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
        performAction:chrome_test_util::LongPressElementForContextMenu(
                          [ElementSelector
                              selectorWithElementID:kSimpleTextElementId],
                          true)];

    id<GREYMatcher> copyButton =
        chrome_test_util::SystemSelectionCalloutCopyButton();
    [ChromeEarlGrey waitForSufficientlyVisibleElementWithMatcher:copyButton];

    // Make sure the Link to Text button is not visible.
    [[EarlGrey selectElementWithMatcher:
                   chrome_test_util::SystemSelectionCalloutLinkToTextButton()]
        assertWithMatcher:grey_notVisible()];
  }
}

- (void)testInputDisablesGenerateLink {
  // In order to make the menu show up later in the test, the pasteboard can't
  // be empty.
  UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
  pasteboard.string = @"anything";

  [ChromeEarlGrey loadURL:self.testServer->GetURL(kInputTestURL)];
  [ChromeEarlGrey waitForWebStateContainingText:kInputTestPageTextSample];

  [ChromeTestCase removeAnyOpenMenusAndInfoBars];

  // Tap to focus the field, then long press to make the Edit Menu pop up.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::TapWebElementWithId(
                        kSimpleTextElementId)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        [ElementSelector
                            selectorWithElementID:kSimpleTextElementId],
                        true)];

  // Ensure the menu is visible by finding the Paste button.
  id<GREYMatcher> menu = grey_accessibilityLabel(@"Paste");
  [EarlGrey selectElementWithMatcher:menu];

  // Make sure the Link to Text button is not visible.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::SystemSelectionCalloutLinkToTextButton()]
      assertWithMatcher:grey_notVisible()];
}

@end
