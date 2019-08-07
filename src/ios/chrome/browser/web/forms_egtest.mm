// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ios/web/public/test/earl_grey/web_view_actions.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ios/web/public/test/element_selector.h"
#include "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ios/web/public/test/url_test_util.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::OmniboxText;
using testing::ElementToDismissAlert;

namespace {

// Response shown on the page of |GetDestinationUrl|.
const char kDestinationText[] = "bar!";

// Response shown on the page of |GetGenericUrl|.
const char kGenericText[] = "A generic page";

// Label for the button in the form.
NSString* kSubmitButtonLabel = @"submit";

// Html form template with a submission button named "submit".
const char* kFormHtmlTemplate =
    "<form method='post' action='%s'> submit: "
    "<input value='textfield' id='textfield' type='text'></label>"
    "<input type='submit' value='submit' id='submit'>"
    "</form>";

// GURL of a generic website in the user navigation flow.
const GURL GetGenericUrl() {
  return web::test::HttpServer::MakeUrl("http://generic");
}

// GURL of a page with a form that posts data to |GetDestinationUrl|.
const GURL GetFormUrl() {
  return web::test::HttpServer::MakeUrl("http://form");
}

// GURL of a page with a form that posts data to |GetDestinationUrl|.
const GURL GetFormPostOnSamePageUrl() {
  return web::test::HttpServer::MakeUrl("http://form");
}

// GURL of the page to which the |GetFormUrl| posts data to.
const GURL GetDestinationUrl() {
  return web::test::HttpServer::MakeUrl("http://destination");
}

#pragma mark - TestFormResponseProvider

// URL that redirects to |GetDestinationUrl| with a 302.
const GURL GetRedirectUrl() {
  return web::test::HttpServer::MakeUrl("http://redirect");
}

// URL to return a page that posts to |GetRedirectUrl|.
const GURL GetRedirectFormUrl() {
  return web::test::HttpServer::MakeUrl("http://formRedirect");
}

// A ResponseProvider that provides html response, post response or a redirect.
class TestFormResponseProvider : public web::DataResponseProvider {
 public:
  // TestResponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;
};

bool TestFormResponseProvider::CanHandleRequest(const Request& request) {
  const GURL& url = request.url;
  return url == GetDestinationUrl() || url == GetRedirectUrl() ||
         url == GetRedirectFormUrl() || url == GetFormPostOnSamePageUrl() ||
         url == GetGenericUrl();
}

void TestFormResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  if (url == GetRedirectUrl()) {
    *headers = web::ResponseProvider::GetRedirectResponseHeaders(
        GetDestinationUrl().spec(), net::HTTP_FOUND);
    return;
  }

  *headers = web::ResponseProvider::GetDefaultResponseHeaders();
  if (url == GetGenericUrl()) {
    *response_body = kGenericText;
    return;
  }
  if (url == GetFormPostOnSamePageUrl()) {
    if (request.method == "POST") {
      *response_body = request.method + std::string(" ") + request.body;
    } else {
      *response_body =
          "<form method='post'>"
          "<input value='button' type='submit' id='button'></form>";
    }
    return;
  }

  if (url == GetRedirectFormUrl()) {
    *response_body =
        base::StringPrintf(kFormHtmlTemplate, GetRedirectUrl().spec().c_str());
    return;
  }
  if (url == GetDestinationUrl()) {
    *response_body = request.method + std::string(" ") + request.body;
    return;
  }
  NOTREACHED();
}

}  // namespace

// Tests submition of HTTP forms POST data including cases involving navigation.
@interface FormsTestCase : ChromeTestCase
@end

@implementation FormsTestCase

// Matcher for a Go button that is interactable.
id<GREYMatcher> GoButtonMatcher() {
  return grey_allOf(grey_accessibilityID(@"Go"), grey_interactable(), nil);
}

// Matcher for the resend POST button in the repost warning dialog.
id<GREYMatcher> ResendPostButtonMatcher() {
  return chrome_test_util::ButtonWithAccessibilityLabelId(
      IDS_HTTP_POST_WARNING_RESEND);
}

// Waits for view with Tab History accessibility ID.
- (void)waitForTabHistoryView {
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Waiting for Tab History to display."
                  block:^BOOL {
                    NSError* error = nil;
                    id<GREYMatcher> tabHistory =
                        grey_accessibilityID(kPopupMenuNavigationTableViewId);
                    [[EarlGrey selectElementWithMatcher:tabHistory]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return error == nil;
                  }];
  GREYAssert(
      [condition waitWithTimeout:base::test::ios::kWaitForUIElementTimeout],
      @"Tab History View not displayed.");
}

// Open back navigation history.
- (void)openBackHistory {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_longPress()];
}

// Accepts the warning that the form POST data will be reposted.
- (void)confirmResendWarning {
  [[EarlGrey selectElementWithMatcher:ResendPostButtonMatcher()]
      performAction:grey_longPress()];
}

// Sets up a basic simple http server for form test with a form located at
// |GetFormUrl|, and posts data to |GetDestinationUrl| upon submission.
- (void)setUpFormTestSimpleHttpServer {
  std::map<GURL, std::string> responses;
  responses[GetGenericUrl()] = kGenericText;
  responses[GetFormUrl()] =
      base::StringPrintf(kFormHtmlTemplate, GetDestinationUrl().spec().c_str());
  responses[GetDestinationUrl()] = kDestinationText;
  web::test::SetUpSimpleHttpServer(responses);
}

// Tests that a POST followed by reloading the destination page resends data.
- (void)testRepostFormAfterReload {
  if (IsIPadIdiom() && web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // TODO(crbug.com/968296): Re-enable this test on iPad.
    EARL_GREY_TEST_DISABLED(@"Test disabled with Slim Navigation.");
  }

  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetFormUrl()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // WKBasedNavigationManager presents repost confirmation dialog before loading
  // stops.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC() reload];
  } else {
    // Legacy navigation manager presents repost confirmation dialog after
    // loading stops.
    CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey reload]);
  }
  [self confirmResendWarning];

  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a POST followed by navigating to a new page and then tapping back
// to the form result page resends data.
- (void)testRepostFormAfterTappingBack {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetFormUrl()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go to a new page and go back and check that the data is reposted.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetGenericUrl()]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goBack]);

  // WKBasedNavigationManager doesn't triggere repost on |goForward| due to
  // WKWebView's back-forward cache. Force reload to trigger repost. Not using
  // [ChromeEarlGrey reload] because WKBasedNavigationManager presents repost
  // confirmation dialog before loading stops.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC() reload];
  }

  [self confirmResendWarning];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a POST followed by tapping back to the form page and then tapping
// forward to the result page resends data.
- (void)testRepostFormAfterTappingBackAndForward {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetFormUrl()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goBack]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goForward]);

  // WKBasedNavigationManager doesn't triggere repost on |goForward| due to
  // WKWebView's back-forward cache. Force reload to trigger repost. Not using
  // [ChromeEarlGrey reload] because WKBasedNavigationManager presents repost
  // confirmation dialog before loading stops.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC() reload];
  }

  [self confirmResendWarning];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a POST followed by a new request and then index navigation to get
// back to the result page resends data.
- (void)testRepostFormAfterIndexNavigation {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetFormUrl()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go to a new page and go back to destination through back history.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetGenericUrl()]);
  [self openBackHistory];
  [self waitForTabHistoryView];
  id<GREYMatcher> historyItem = grey_text(
      base::SysUTF16ToNSString(web::GetDisplayTitleForUrl(destinationURL)));
  [[EarlGrey selectElementWithMatcher:historyItem] performAction:grey_tap()];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForPageToFinishLoading]);

  // Back-forward navigation with WKBasedNavigationManager is served from
  // WKWebView's app-cache, so it won't trigger repost warning.
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    [self confirmResendWarning];
  }

  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// When data is not reposted, the request is canceled.
- (void)testRepostFormCancelling {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetFormUrl()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goBack]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goForward]);

  // WKBasedNavigationManager doesn't triggere repost on |goForward| due to
  // WKWebView's back-forward cache. Force reload to trigger repost. Not using
  // [ChromeEarlGrey reload] because WKBasedNavigationManager presents repost
  // confirmation dialog before loading stops.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC() reload];
  }

  [[EarlGrey selectElementWithMatcher:ElementToDismissAlert(@"Cancel")]
      performAction:grey_tap()];
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey waitForPageToFinishLoading]);

  // Expected behavior is different between the two navigation manager
  // implementations.
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    // LegacyNavigationManager displays repost on |goBack|. So after cancelling,
    // web view should show form URL.
    CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
        waitForWebStateContainingText:(base::SysNSStringToUTF8(
                                          kSubmitButtonLabel))]);
    [[EarlGrey selectElementWithMatcher:OmniboxText(GetFormUrl().GetContent())]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
        assertWithMatcher:grey_interactable()];
  } else {
    // WKBasedNavigationManager displays repost on |reload|. So after
    // cancelling, web view should show |destinationURL|.
    CHROME_EG_ASSERT_NO_ERROR(
        [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
    [[EarlGrey
        selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
        assertWithMatcher:grey_notNil()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
        assertWithMatcher:grey_interactable()];
  }
}

// A new navigation dismisses the repost dialog.
- (void)testRepostFormDismissedByNewNavigation {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetFormUrl()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // WKBasedNavigationManager presents repost confirmation dialog before loading
  // stops.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    [chrome_test_util::BrowserCommandDispatcherForMainBVC() reload];
  } else {
    // Legacy navigation manager presents repost confirmation dialog after
    // loading stops.
    CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey reload]);
  }

  // Repost confirmation box should be visible.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForSufficientlyVisibleElementWithMatcher:ResendPostButtonMatcher()]);

  // Starting a new navigation while the repost dialog is presented should not
  // crash.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetGenericUrl()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kGenericText]);

  // Repost dialog should not be visible anymore.
  [[EarlGrey selectElementWithMatcher:ResendPostButtonMatcher()]
      assertWithMatcher:grey_not(grey_sufficientlyVisible())];
}

// Tests that pressing the button on a POST-based form changes the page and that
// the back button works as expected afterwards.
- (void)testGoBackButtonAfterFormSubmission {
  [self setUpFormTestSimpleHttpServer];
  GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetFormUrl()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDestinationText]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back and verify the browser navigates to the original URL.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goBack]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:(base::SysNSStringToUTF8(
                                                        kSubmitButtonLabel))]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFormUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that a POST followed by a redirect does not show the popup.
- (void)testRepostFormCancellingAfterRedirect {
  web::test::SetUpHttpServer(std::make_unique<TestFormResponseProvider>());
  const GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetRedirectFormUrl()]);

  // Submit the form, which redirects before printing the data.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kSubmitButtonLabel]);

  // Check that the redirect changes the POST to a GET.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:"GET"]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey reload]);

  // Check that the popup did not show
  [[EarlGrey selectElementWithMatcher:ResendPostButtonMatcher()]
      assertWithMatcher:grey_nil()];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:"GET"]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that pressing the button on a POST-based form with same-page action
// does not change the page URL and that the back button works as expected
// afterwards.
// TODO(crbug.com/714303): Re-enable this test on devices.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testPostFormToSamePage testPostFormToSamePage
#else
#define MAYBE_testPostFormToSamePage FLAKY_testPostFormToSamePage
#endif
- (void)MAYBE_testPostFormToSamePage {
  web::test::SetUpHttpServer(std::make_unique<TestFormResponseProvider>());
  const GURL formURL = GetFormPostOnSamePageUrl();

  // Open the first URL so it's in history.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetGenericUrl()]);

  // Open the second URL, tap the button, and verify the browser navigates to
  // the expected URL.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:formURL]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:@"button"]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:"POST"]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(formURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back once and verify the browser navigates to the form URL.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goBack]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(formURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back a second time and verify the browser navigates to the first URL.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goBack]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetGenericUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that submitting a POST-based form by tapping the 'Go' button on the
// keyboard navigates to the correct URL and the back button works as expected
// afterwards.
- (void)testPostFormEntryWithKeyboard {
  [self setUpFormTestSimpleHttpServer];
  const GURL destinationURL = GetDestinationUrl();

  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GetFormUrl()]);
  [self submitFormUsingKeyboardGoButtonWithInputID:"textfield"];

  // Verify that the browser navigates to the expected URL.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:"bar!"]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  // Go back and verify that the browser navigates to the original URL.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey goBack]);
  [[EarlGrey selectElementWithMatcher:OmniboxText(GetFormUrl().GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tap the text field indicated by |ID| to open the keyboard, and then
// press the keyboard's "Go" button to submit the form.
- (void)submitFormUsingKeyboardGoButtonWithInputID:(const std::string&)ID {
  // Disable EarlGrey's synchronization since it is blocked by opening the
  // keyboard from a web view.
  [[GREYConfiguration sharedInstance]
          setValue:@NO
      forConfigKey:kGREYConfigKeySynchronizationEnabled];

  // Wait for web view to be interactable before tapping.
  GREYCondition* interactableCondition = [GREYCondition
      conditionWithName:@"Wait for web view to be interactable."
                  block:^BOOL {
                    NSError* error = nil;
                    id<GREYMatcher> webViewMatcher = WebViewInWebState(
                        chrome_test_util::GetCurrentWebState());
                    [[EarlGrey selectElementWithMatcher:webViewMatcher]
                        assertWithMatcher:grey_interactable()
                                    error:&error];
                    return !error;
                  }];
  GREYAssert([interactableCondition
                 waitWithTimeout:base::test::ios::kWaitForUIElementTimeout],
             @"Web view did not become interactable.");

  web::WebState* currentWebState = chrome_test_util::GetCurrentWebState();
  [[EarlGrey selectElementWithMatcher:web::WebViewInWebState(currentWebState)]
      performAction:web::WebViewTapElement(
                        currentWebState,
                        [ElementSelector selectorWithElementID:ID])];

  // Wait until the keyboard shows up before tapping.
  GREYCondition* condition = [GREYCondition
      conditionWithName:@"Wait for the keyboard to show up."
                  block:^BOOL {
                    NSError* error = nil;
                    [[EarlGrey selectElementWithMatcher:GoButtonMatcher()]
                        assertWithMatcher:grey_notNil()
                                    error:&error];
                    return (error == nil);
                  }];
  GREYAssert(
      [condition waitWithTimeout:base::test::ios::kWaitForUIElementTimeout],
      @"No keyboard with 'Go' button showed up.");

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Go")]
      performAction:grey_tap()];

  // Reenable synchronization now that the keyboard has been closed.
  [[GREYConfiguration sharedInstance]
          setValue:@YES
      forConfigKey:kGREYConfigKeySynchronizationEnabled];
}

@end
