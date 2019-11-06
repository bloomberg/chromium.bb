// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_controller.h"

#import <WebKit/WebKit.h>

#include <memory>
#include <utility>

#include "base/mac/foundation_util.h"
#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/common/crw_content_view.h"
#import "ios/web/common/crw_web_view_content_view.h"
#include "ios/web/common/features.h"
#import "ios/web/js_messaging/crw_js_injector.h"
#import "ios/web/js_messaging/web_view_js_utils.h"
#include "ios/web/navigation/block_universal_links_buildflags.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_wk_navigation_states.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_action_policy_util.h"
#import "ios/web/public/deprecated/crw_native_content.h"
#import "ios/web/public/deprecated/crw_native_content_holder.h"
#import "ios/web/public/deprecated/crw_native_content_provider.h"
#import "ios/web/public/deprecated/test_native_content.h"
#import "ios/web/public/deprecated/test_native_content_provider.h"
#include "ios/web/public/deprecated/url_verification_constants.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#include "ios/web/public/navigation/referrer.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#include "ios/web/public/test/fakes/fake_download_controller_delegate.h"
#import "ios/web/public/test/fakes/fake_web_state_policy_decider.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/fakes/test_web_state_delegate.h"
#include "ios/web/public/test/fakes/test_web_state_observer.h"
#import "ios/web/public/test/fakes/test_web_view_content_view.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#import "ios/web/test/fakes/crw_fake_wk_frame_info.h"
#import "ios/web/test/fakes/crw_fake_wk_navigation_action.h"
#import "ios/web/test/fakes/crw_fake_wk_navigation_response.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_test_with_web_controller.h"
#import "ios/web/test/wk_web_view_crash_utils.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_controller_container_view.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"
#include "net/cert/x509_util_ios_and_mac.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {
namespace {

// Syntactically invalid URL per rfc3986.
const char kInvalidURL[] = "http://%3";

const char kTestDataURL[] = "data:text/html,";

const char kTestURLString[] = "http://www.google.com/";
const char kTestAppSpecificURL[] = "testwebui://test/";

const char kTestMimeType[] = "application/vnd.test";

// Returns HTML for an optionally zoomable test page with |zoom_state|.
enum PageScalabilityType {
  PAGE_SCALABILITY_DISABLED = 0,
  PAGE_SCALABILITY_ENABLED,
};

// Tests in this file are parameterized on this enum to test both
// LegacyNavigationManagerImpl and WKBasedNavigationManagerImpl.
enum class NavigationManagerChoice {
  LEGACY,
  WK_BASED,
};

// A mixin class that encapsulates the parameterization of navigation manager
// choice.
class ProgrammaticTestMixin
    : public ::testing::WithParamInterface<NavigationManagerChoice> {
 public:
  void SetUp() {
    if (GetParam() == NavigationManagerChoice::LEGACY) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          features::kSlimNavigationManager);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Base class for WebTestWithWebState tests to enable navigation manager mixin.
class ProgrammaticWebTestWithWebState : public WebTestWithWebState,
                                        public ProgrammaticTestMixin {
 public:
  void SetUp() override {
    ProgrammaticTestMixin::SetUp();
    WebTestWithWebState::SetUp();
  }
};

// Base class for WebTestWithWebController tests to enable navigation manager
// mixin.
class ProgrammaticWebTestWithWebController : public WebTestWithWebController,
                                             public ProgrammaticTestMixin {
 public:
  void SetUp() override {
    ProgrammaticTestMixin::SetUp();
    WebTestWithWebState::SetUp();
  }
};

// Macro to simplify instantiation of parameterized tests.
#define INSTANTIATE_TEST_SUITES(cls)                     \
  INSTANTIATE_TEST_SUITE_P(                              \
      Programmatic##cls, cls,                            \
      ::testing::Values(NavigationManagerChoice::LEGACY, \
                        NavigationManagerChoice::WK_BASED))

}  // namespace

// Test fixture for testing CRWWebController. Stubs out web view.
class CRWWebControllerTest : public WebTestWithWebController,
                             public ProgrammaticTestMixin {
 protected:
  CRWWebControllerTest()
      : WebTestWithWebController(std::make_unique<TestWebClient>()) {}

  void SetUp() override {
    ProgrammaticTestMixin::SetUp();
    WebTestWithWebController::SetUp();
    fake_wk_list_ = [[CRWFakeBackForwardList alloc] init];
    mock_web_view_ = CreateMockWebView(fake_wk_list_);
    scroll_view_ = [[UIScrollView alloc] init];
    SetWebViewURL(@(kTestURLString));
    [[[mock_web_view_ stub] andReturn:scroll_view_] scrollView];

    TestWebViewContentView* web_view_content_view =
        [[TestWebViewContentView alloc] initWithMockWebView:mock_web_view_
                                                 scrollView:scroll_view_];
    [web_controller() injectWebViewContentView:web_view_content_view];
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_web_view_);
    [web_controller() resetInjectedWebViewContentView];
    WebTestWithWebController::TearDown();
  }

  TestWebClient* GetWebClient() override {
    return static_cast<TestWebClient*>(
        WebTestWithWebController::GetWebClient());
  }

  // The value for web view OCMock objects to expect for |-setFrame:|.
  CGRect GetExpectedWebViewFrame() const {
    CGSize container_view_size =
        UIApplication.sharedApplication.keyWindow.bounds.size;
    container_view_size.height -=
        CGRectGetHeight([UIApplication sharedApplication].statusBarFrame);
    return {CGPointZero, container_view_size};
  }

  void SetWebViewURL(NSString* url_string) {
    test_url_ = [NSURL URLWithString:url_string];
  }

  // Creates WebView mock.
  UIView* CreateMockWebView(CRWFakeBackForwardList* wk_list) {
    id result = [OCMockObject mockForClass:[WKWebView class]];

    OCMStub([result backForwardList]).andReturn(wk_list);
    // This uses |andDo| rather than |andReturn| since the URL it returns needs
    // to change when |test_url_| changes.
    OCMStub([result URL]).andDo(^(NSInvocation* invocation) {
      [invocation setReturnValue:&test_url_];
    });
    OCMStub(
        [result setNavigationDelegate:[OCMArg checkWithBlock:^(id delegate) {
                  navigation_delegate_ = delegate;
                  return YES;
                }]]);
    OCMStub([result serverTrust]);
    OCMStub([result setUIDelegate:OCMOCK_ANY]);
    OCMStub([result frame]).andReturn(UIScreen.mainScreen.bounds);
    OCMStub([result setCustomUserAgent:OCMOCK_ANY]);
    OCMStub([result customUserAgent]);
    OCMStub([static_cast<WKWebView*>(result) loadRequest:OCMOCK_ANY]);
    OCMStub([result setFrame:GetExpectedWebViewFrame()]);
    OCMStub([result addObserver:OCMOCK_ANY
                     forKeyPath:OCMOCK_ANY
                        options:0
                        context:nullptr]);
    OCMStub([result removeObserver:OCMOCK_ANY forKeyPath:OCMOCK_ANY]);
    OCMStub([result evaluateJavaScript:OCMOCK_ANY
                     completionHandler:OCMOCK_ANY]);
    OCMStub([result allowsBackForwardNavigationGestures]);
    OCMStub([result setAllowsBackForwardNavigationGestures:NO]);
    OCMStub([result setAllowsBackForwardNavigationGestures:YES]);
    OCMStub([result isLoading]);
    OCMStub([result stopLoading]);
    OCMStub([result removeFromSuperview]);
    OCMStub([result hasOnlySecureContent]).andReturn(YES);
    OCMStub([(WKWebView*)result title]).andReturn(@"Title");

    return result;
  }

  __weak id<WKNavigationDelegate> navigation_delegate_;
  UIScrollView* scroll_view_;
  id mock_web_view_;
  CRWFakeBackForwardList* fake_wk_list_;
  NSURL* test_url_;
};

// Tests that AllowCertificateError is called with correct arguments if
// WKWebView fails to load a page with bad SSL cert.
TEST_P(CRWWebControllerTest, SslCertError) {
  // Last arguments passed to AllowCertificateError must be in default state.
  ASSERT_FALSE(GetWebClient()->last_cert_error_code());
  ASSERT_FALSE(GetWebClient()->last_cert_error_ssl_info().is_valid());
  ASSERT_FALSE(GetWebClient()->last_cert_error_ssl_info().cert_status);
  ASSERT_FALSE(GetWebClient()->last_cert_error_request_url().is_valid());
  ASSERT_TRUE(GetWebClient()->last_cert_error_overridable());

  scoped_refptr<net::X509Certificate> cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  ASSERT_TRUE(cert);
  base::ScopedCFTypeRef<CFMutableArrayRef> chain(
      net::x509_util::CreateSecCertificateArrayForX509Certificate(cert.get()));
  ASSERT_TRUE(chain);

  GURL url("https://chromium.test");
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:@{
                        kNSErrorPeerCertificateChainKey :
                            base::mac::CFToNSCast(chain.get()),
                        kNSErrorFailingURLKey : net::NSURLWithGURL(url),
                      }];
  NSObject* navigation = [[NSObject alloc] init];
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:static_cast<WKNavigation*>(navigation)];
  [navigation_delegate_ webView:mock_web_view_
      didFailProvisionalNavigation:static_cast<WKNavigation*>(navigation)
                         withError:error];

  // Verify correctness of AllowCertificateError method call.
  EXPECT_EQ(net::ERR_CERT_INVALID, GetWebClient()->last_cert_error_code());
  EXPECT_TRUE(GetWebClient()->last_cert_error_ssl_info().is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID,
            GetWebClient()->last_cert_error_ssl_info().cert_status);
  EXPECT_EQ(url, GetWebClient()->last_cert_error_request_url());
  EXPECT_FALSE(GetWebClient()->last_cert_error_overridable());
}

// Tests that when a committed but not-yet-finished navigation is cancelled,
// the navigation item's ErrorRetryStateMachine is updated correctly.
TEST_P(CRWWebControllerTest, CancelCommittedNavigation) {
  [[[mock_web_view_ stub] andReturnBool:NO] hasOnlySecureContent];
  [static_cast<WKWebView*>([[mock_web_view_ stub] andReturn:@""]) title];

  WKNavigation* navigation =
      static_cast<WKNavigation*>([[NSObject alloc] init]);
  SetWebViewURL(@"http://chromium.test");
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:navigation];
  [fake_wk_list_ setCurrentURL:@"http://chromium.test"];
  [navigation_delegate_ webView:mock_web_view_ didCommitNavigation:navigation];
  NSError* error = [NSError errorWithDomain:NSURLErrorDomain
                                       code:NSURLErrorCancelled
                                   userInfo:nil];
  [navigation_delegate_ webView:mock_web_view_
              didFailNavigation:navigation
                      withError:error];
  NavigationManagerImpl& navigation_manager =
      web_controller().webStateImpl->GetNavigationManagerImpl();
  NavigationItemImpl* item = navigation_manager.GetLastCommittedItemImpl();
  EXPECT_EQ(ErrorRetryState::kNoNavigationError,
            item->error_retry_state_machine().state());
}

// Tests that when a placeholder navigation is preempted by another navigation,
// WebStateObservers get neither a DidStartNavigation nor a DidFinishNavigation
// call for the corresponding native URL navigation.
TEST_P(CRWWebControllerTest, AbortNativeUrlNavigation) {
  // The legacy navigation manager doesn't have the concept of placeholder
  // navigations.
  if (!GetWebClient()->IsSlimNavigationManagerEnabled())
    return;
  GURL native_url(
      url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  NSString* placeholder_url = [NSString
      stringWithFormat:@"%s%s", "about:blank?for=", native_url.spec().c_str()];
  TestWebStateObserver observer(web_state());

  WKNavigation* navigation =
      static_cast<WKNavigation*>([[NSObject alloc] init]);
  [static_cast<WKWebView*>([[mock_web_view_ stub] andReturn:navigation])
      loadRequest:OCMOCK_ANY];
  TestNativeContentProvider* mock_native_provider =
      [[TestNativeContentProvider alloc] init];
  TestNativeContent* content =
      [[TestNativeContent alloc] initWithURL:native_url virtualURL:native_url];
  [mock_native_provider setController:content forURL:native_url];
  [[web_controller() nativeContentHolder]
      setNativeProvider:mock_native_provider];

  AddPendingItem(native_url, ui::PAGE_TRANSITION_TYPED);

  // Trigger a placeholder navigation.
  [web_controller() loadCurrentURLWithRendererInitiatedNavigation:NO];

  // Simulate the WKNavigationDelegate callbacks for the placeholder navigation
  // arriving after another pending item has already been created.
  AddPendingItem(GURL(kTestURLString), ui::PAGE_TRANSITION_TYPED);
  SetWebViewURL(placeholder_url);
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:navigation];
  [navigation_delegate_ webView:mock_web_view_ didCommitNavigation:navigation];
  [navigation_delegate_ webView:mock_web_view_ didFinishNavigation:navigation];

  EXPECT_FALSE(observer.did_start_navigation_info());
  EXPECT_FALSE(observer.did_finish_navigation_info());
}

// Tests returning pending item stored in navigation context.
TEST_P(CRWWebControllerTest, TestPendingItem) {
  ASSERT_FALSE([web_controller() pendingItemForSessionController:nil]);
  ASSERT_FALSE([web_controller() lastPendingItemForNewNavigation]);
  ASSERT_FALSE(web_controller().webStateImpl->GetPendingItem());

  // Create pending item by simulating a renderer-initiated navigation.
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:nil];

  NavigationItemImpl* item = [web_controller() lastPendingItemForNewNavigation];

  // Verify that the same item is returned by NavigationManagerDelegate,
  // CRWSessionControllerDelegate and CRWWebController.
  ASSERT_TRUE(item);
  EXPECT_EQ(item, [web_controller() pendingItemForSessionController:nil]);
  EXPECT_EQ(item, web_controller().webStateImpl->GetPendingItem());

  EXPECT_EQ(kTestURLString, item->GetURL());
}

// Tests allowsBackForwardNavigationGestures default value and negating this
// property.
TEST_P(CRWWebControllerTest, SetAllowsBackForwardNavigationGestures) {
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_TRUE(web_controller().allowsBackForwardNavigationGestures);
    web_controller().allowsBackForwardNavigationGestures = NO;
    EXPECT_FALSE(web_controller().allowsBackForwardNavigationGestures);
  } else {
    EXPECT_FALSE(web_controller().allowsBackForwardNavigationGestures);
    web_controller().allowsBackForwardNavigationGestures = YES;
    EXPECT_TRUE(web_controller().allowsBackForwardNavigationGestures);
  }
}

// Tests that the navigation state is reset to FINISHED when a back/forward
// navigation occurs during a pending navigation.
TEST_P(CRWWebControllerTest, BackForwardWithPendingNavigation) {
  ASSERT_FALSE([web_controller() pendingItemForSessionController:nil]);
  ASSERT_FALSE([web_controller() lastPendingItemForNewNavigation]);
  ASSERT_FALSE(web_controller().webStateImpl->GetPendingItem());

  // Commit a navigation so that there is a back NavigationItem.
  SetWebViewURL(@"about:blank");
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:nil];
  [navigation_delegate_ webView:mock_web_view_ didCommitNavigation:nil];
  [navigation_delegate_ webView:mock_web_view_ didFinishNavigation:nil];

  // Create pending item by simulating a renderer-initiated navigation.
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:nil];
  ASSERT_EQ(web::WKNavigationState::REQUESTED,
            web_controller().navigationState);

  [web_controller() didFinishGoToIndexSameDocumentNavigationWithType:
                        web::NavigationInitiationType::BROWSER_INITIATED
                                                      hasUserGesture:YES];
  EXPECT_EQ(web::WKNavigationState::FINISHED, web_controller().navigationState);
}

INSTANTIATE_TEST_SUITES(CRWWebControllerTest);

// Test fixture to test JavaScriptDialogPresenter.
class JavaScriptDialogPresenterTest : public ProgrammaticWebTestWithWebState {
 protected:
  JavaScriptDialogPresenterTest() : page_url_("https://chromium.test/") {}
  void SetUp() override {
    ProgrammaticWebTestWithWebState::SetUp();
    LoadHtml(@"<html><body></body></html>", page_url_);
    web_state()->SetDelegate(&test_web_delegate_);
  }
  void TearDown() override {
    web_state()->SetDelegate(nullptr);
    ProgrammaticWebTestWithWebState::TearDown();
  }
  TestJavaScriptDialogPresenter* js_dialog_presenter() {
    return test_web_delegate_.GetTestJavaScriptDialogPresenter();
  }
  const std::vector<std::unique_ptr<TestJavaScriptDialog>>&
  requested_dialogs() {
    return js_dialog_presenter()->requested_dialogs();
  }
  const GURL& page_url() { return page_url_; }

 private:
  TestWebStateDelegate test_web_delegate_;
  GURL page_url_;
};

// Tests that window.alert dialog is shown.
TEST_P(JavaScriptDialogPresenterTest, Alert) {
  ASSERT_TRUE(requested_dialogs().empty());

  ExecuteJavaScript(@"alert('test')");

  ASSERT_EQ(1U, requested_dialogs().size());
  auto& dialog = requested_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_EQ(JAVASCRIPT_DIALOG_TYPE_ALERT, dialog->java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog->message_text);
  EXPECT_FALSE(dialog->default_prompt_text);
}

// Tests that window.confirm dialog is shown and its result is true.
TEST_P(JavaScriptDialogPresenterTest, ConfirmWithTrue) {
  ASSERT_TRUE(requested_dialogs().empty());

  js_dialog_presenter()->set_callback_success_argument(true);

  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  auto& dialog = requested_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_EQ(JAVASCRIPT_DIALOG_TYPE_CONFIRM, dialog->java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog->message_text);
  EXPECT_FALSE(dialog->default_prompt_text);
}

// Tests that window.confirm dialog is shown and its result is false.
TEST_P(JavaScriptDialogPresenterTest, ConfirmWithFalse) {
  ASSERT_TRUE(requested_dialogs().empty());

  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"confirm('test')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  auto& dialog = requested_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_EQ(JAVASCRIPT_DIALOG_TYPE_CONFIRM, dialog->java_script_dialog_type);
  EXPECT_NSEQ(@"test", dialog->message_text);
  EXPECT_FALSE(dialog->default_prompt_text);
}

// Tests that window.prompt dialog is shown.
TEST_P(JavaScriptDialogPresenterTest, Prompt) {
  ASSERT_TRUE(requested_dialogs().empty());

  js_dialog_presenter()->set_callback_user_input_argument(@"Maybe");

  EXPECT_NSEQ(@"Maybe", ExecuteJavaScript(@"prompt('Yes?', 'No')"));

  ASSERT_EQ(1U, requested_dialogs().size());
  auto& dialog = requested_dialogs().front();
  EXPECT_EQ(web_state(), dialog->web_state);
  EXPECT_EQ(page_url(), dialog->origin_url);
  EXPECT_EQ(JAVASCRIPT_DIALOG_TYPE_PROMPT, dialog->java_script_dialog_type);
  EXPECT_NSEQ(@"Yes?", dialog->message_text);
  EXPECT_NSEQ(@"No", dialog->default_prompt_text);
}

INSTANTIATE_TEST_SUITES(JavaScriptDialogPresenterTest);

// Test fixture for testing visible security state.
typedef ProgrammaticWebTestWithWebState CRWWebStateSecurityStateTest;

// Tests that loading HTTP page updates the SSLStatus.
TEST_P(CRWWebStateSecurityStateTest, LoadHttpPage) {
  TestWebStateObserver observer(web_state());
  ASSERT_FALSE(observer.did_change_visible_security_state_info());
  LoadHtml(@"<html><body></body></html>", GURL("http://chromium.test"));
  NavigationManager* nav_manager = web_state()->GetNavigationManager();
  NavigationItem* item = nav_manager->GetLastCommittedItem();
  EXPECT_EQ(SECURITY_STYLE_UNAUTHENTICATED, item->GetSSL().security_style);
  ASSERT_TRUE(observer.did_change_visible_security_state_info());
  EXPECT_EQ(web_state(),
            observer.did_change_visible_security_state_info()->web_state);
}

INSTANTIATE_TEST_SUITES(CRWWebStateSecurityStateTest);

// Real WKWebView is required for CRWWebControllerInvalidUrlTest.
typedef ProgrammaticWebTestWithWebState CRWWebControllerInvalidUrlTest;

// Tests that web controller does not navigate to about:blank if iframe src
// has invalid url. Web controller loads about:blank if page navigates to
// invalid url, but should do nothing if navigation is performed in iframe. This
// test prevents crbug.com/694865 regression.
TEST_P(CRWWebControllerInvalidUrlTest, IFrameWithInvalidURL) {
  GURL url("http://chromium.test");
  ASSERT_FALSE(GURL(kInvalidURL).is_valid());
  LoadHtml([NSString stringWithFormat:@"<iframe src='%s'/>", kInvalidURL], url);
  EXPECT_EQ(url, web_state()->GetLastCommittedURL());
}

INSTANTIATE_TEST_SUITES(CRWWebControllerInvalidUrlTest);

// Real WKWebView is required for CRWWebControllerMessageFromIFrame.
typedef ProgrammaticWebTestWithWebState CRWWebControllerMessageFromIFrame;

// Tests that invalid message from iframe does not cause a crash.
TEST_P(CRWWebControllerMessageFromIFrame, InvalidMessage) {
  static NSString* const kHTMLIFrameSendsInvalidMessage =
      @"<body><iframe name='f'></iframe></body>";

  LoadHtml(kHTMLIFrameSendsInvalidMessage);

  // Sending unknown command from iframe should not cause a crash.
  ExecuteJavaScript(
      @"var bad_message = {'command' : 'unknown.command'};"
       "frames['f'].__gCrWeb.message.invokeOnHost(bad_message);");
}

INSTANTIATE_TEST_SUITES(CRWWebControllerMessageFromIFrame);

// Real WKWebView is required for CRWWebControllerJSExecutionTest.
typedef ProgrammaticWebTestWithWebController CRWWebControllerJSExecutionTest;

// Tests that a script correctly evaluates to boolean.
TEST_P(CRWWebControllerJSExecutionTest, Execution) {
  LoadHtml(@"<p></p>");
  EXPECT_NSEQ(@YES, ExecuteJavaScript(@"true"));
  EXPECT_NSEQ(@NO, ExecuteJavaScript(@"false"));
}

// Tests that a script is not executed on windowID mismatch.
TEST_P(CRWWebControllerJSExecutionTest, WindowIdMissmatch) {
  LoadHtml(@"<p></p>");
  // Script is evaluated since windowID is matched.
  ExecuteJavaScript(@"window.test1 = '1';");
  EXPECT_NSEQ(@"1", ExecuteJavaScript(@"window.test1"));

  // Change windowID.
  ExecuteJavaScript(@"__gCrWeb['windowId'] = '';");

  // Script is not evaluated because of windowID mismatch.
  ExecuteJavaScript(@"window.test2 = '2';");
  EXPECT_FALSE(ExecuteJavaScript(@"window.test2"));
}

INSTANTIATE_TEST_SUITES(CRWWebControllerJSExecutionTest);

// Test fixture to test decidePolicyForNavigationResponse:decisionHandler:
// delegate method.
class CRWWebControllerResponseTest : public CRWWebControllerTest {
 protected:
  CRWWebControllerResponseTest() : download_delegate_(download_controller()) {}

  // Calls webView:decidePolicyForNavigationResponse:decisionHandler: callback
  // and waits for decision handler call. Returns false if decision handler call
  // times out.
  bool CallDecidePolicyForNavigationResponseWithResponse(
      NSURLResponse* response,
      BOOL for_main_frame,
      BOOL can_show_mime_type,
      WKNavigationResponsePolicy* out_policy) WARN_UNUSED_RESULT {
    CRWFakeWKNavigationResponse* navigation_response =
        [[CRWFakeWKNavigationResponse alloc] init];
    navigation_response.response = response;
    navigation_response.forMainFrame = for_main_frame;
    navigation_response.canShowMIMEType = can_show_mime_type;

    // Call decidePolicyForNavigationResponse and wait for decisionHandler's
    // callback.
    __block bool callback_called = false;
    if (for_main_frame) {
      // webView:didStartProvisionalNavigation: is not called for iframes.
      [navigation_delegate_ webView:mock_web_view_
          didStartProvisionalNavigation:nil];
    }
    [navigation_delegate_ webView:mock_web_view_
        decidePolicyForNavigationResponse:navigation_response
                          decisionHandler:^(WKNavigationResponsePolicy policy) {
                            *out_policy = policy;
                            callback_called = true;
                          }];
    return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
      return callback_called;
    });
  }

  DownloadController* download_controller() {
    return DownloadController::FromBrowserState(GetBrowserState());
  }

  FakeDownloadControllerDelegate download_delegate_;
};

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: allows
// renderer-initiated navigations in main frame for http: URLs.
TEST_P(CRWWebControllerResponseTest, AllowRendererInitiatedResponse) {
  // Simulate regular navigation response with text/html MIME type.
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestURLString)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyCancel;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/YES, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyAllow, policy);

  // Verify that download task was not created for html response.
  ASSERT_TRUE(download_delegate_.alive_download_tasks().empty());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: allows
// renderer-initiated navigations in iframe for data: URLs.
TEST_P(CRWWebControllerResponseTest,
       AllowRendererInitiatedDataUrlResponseInIFrame) {
  // Simulate data:// url response with text/html MIME type.
  SetWebViewURL(@(kTestDataURL));
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestDataURL)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyCancel;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/NO, /*can_show_mime_type=*/YES, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyAllow, policy);

  // Verify that download task was not created for html response.
  ASSERT_TRUE(download_delegate_.alive_download_tasks().empty());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: blocks
// rendering data URLs for renderer-initiated navigations in main frame to
// prevent abusive behavior (crbug.com/890558) and presents the download option.
TEST_P(CRWWebControllerResponseTest,
       DownloadRendererInitiatedDataUrlResponseInMainFrame) {
  // Simulate data:// url response with text/html MIME type.
  SetWebViewURL(@(kTestDataURL));
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestDataURL)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/YES, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyCancel, policy);

  // Verify that download task was created (see crbug.com/949114).
  ASSERT_EQ(1U, download_delegate_.alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_.alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIndentifier());
  EXPECT_EQ(kTestDataURL, task->GetOriginalUrl());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_TRUE(task->GetContentDisposition().empty());
  EXPECT_TRUE(task->GetMimeType().empty());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      task->GetTransitionType(),
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT));
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: allows
// rendering data URLs for browser-initiated navigations in main frame.
TEST_P(CRWWebControllerResponseTest,
       AllowBrowserInitiatedDataUrlResponseInMainFrame) {
  // Simulate data:// url response with text/html MIME type.
  GURL url(kTestDataURL);
  AddPendingItem(url, ui::PAGE_TRANSITION_TYPED);
  [web_controller() loadCurrentURLWithRendererInitiatedNavigation:NO];
  SetWebViewURL(@(kTestDataURL));
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestDataURL)]
        statusCode:200
       HTTPVersion:nil
      headerFields:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyCancel;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/YES, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyAllow, policy);

  // Verify that download task was not created for html response.
  ASSERT_TRUE(download_delegate_.alive_download_tasks().empty());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: creates
// the DownloadTask for NSURLResponse.
TEST_P(CRWWebControllerResponseTest, DownloadWithNSURLResponse) {
  // Simulate download response.
  int64_t content_length = 10;
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@(kTestURLString)]
                                MIMEType:@(kTestMimeType)
                   expectedContentLength:content_length
                        textEncodingName:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyCancel, policy);

  // Verify that download task was created.
  ASSERT_EQ(1U, download_delegate_.alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_.alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIndentifier());
  EXPECT_EQ(kTestURLString, task->GetOriginalUrl());
  EXPECT_EQ(content_length, task->GetTotalBytes());
  EXPECT_EQ("", task->GetContentDisposition());
  EXPECT_EQ(kTestMimeType, task->GetMimeType());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      task->GetTransitionType(),
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT));
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: creates
// the DownloadTask for NSHTTPURLResponse.
TEST_P(CRWWebControllerResponseTest, DownloadWithNSHTTPURLResponse) {
  // Simulate download response.
  const char kContentDisposition[] = "attachment; filename=download.test";
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestURLString)]
        statusCode:200
       HTTPVersion:nil
      headerFields:@{
        @"content-disposition" : @(kContentDisposition),
      }];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyCancel, policy);

  // Verify that download task was created.
  ASSERT_EQ(1U, download_delegate_.alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_.alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIndentifier());
  EXPECT_EQ(kTestURLString, task->GetOriginalUrl());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_EQ(kContentDisposition, task->GetContentDisposition());
  EXPECT_EQ("", task->GetMimeType());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      task->GetTransitionType(),
      ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT));
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler:
// discards pending URL.
TEST_P(CRWWebControllerResponseTest, DownloadDiscardsPendingUrl) {
  GURL url(kTestURLString);
  AddPendingItem(url, ui::PAGE_TRANSITION_TYPED);

  // Simulate download response.
  NSURLResponse* response =
      [[NSURLResponse alloc] initWithURL:[NSURL URLWithString:@(kTestURLString)]
                                MIMEType:@(kTestMimeType)
                   expectedContentLength:10
                        textEncodingName:nil];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/YES, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyCancel, policy);

  // Verify that download task was created and pending URL discarded.
  ASSERT_EQ(1U, download_delegate_.alive_download_tasks().size());
  EXPECT_EQ("", web_state()->GetVisibleURL());
}

// Tests that webView:decidePolicyForNavigationResponse:decisionHandler: creates
// the DownloadTask for NSHTTPURLResponse and iframes.
TEST_P(CRWWebControllerResponseTest, IFrameDownloadWithNSHTTPURLResponse) {
  // Simulate download response.
  const char kContentDisposition[] = "attachment; filename=download.test";
  NSURLResponse* response = [[NSHTTPURLResponse alloc]
       initWithURL:[NSURL URLWithString:@(kTestURLString)]
        statusCode:200
       HTTPVersion:nil
      headerFields:@{
        @"content-disposition" : @(kContentDisposition),
      }];
  WKNavigationResponsePolicy policy = WKNavigationResponsePolicyAllow;
  ASSERT_TRUE(CallDecidePolicyForNavigationResponseWithResponse(
      response, /*for_main_frame=*/NO, /*can_show_mime_type=*/NO, &policy));
  EXPECT_EQ(WKNavigationResponsePolicyCancel, policy);

  // Verify that download task was created.
  ASSERT_EQ(1U, download_delegate_.alive_download_tasks().size());
  DownloadTask* task =
      download_delegate_.alive_download_tasks()[0].second.get();
  ASSERT_TRUE(task);
  EXPECT_TRUE(task->GetIndentifier());
  EXPECT_EQ(kTestURLString, task->GetOriginalUrl());
  EXPECT_EQ(-1, task->GetTotalBytes());
  EXPECT_EQ(kContentDisposition, task->GetContentDisposition());
  EXPECT_EQ("", task->GetMimeType());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      task->GetTransitionType(),
      ui::PageTransition::PAGE_TRANSITION_AUTO_SUBFRAME));
}

// Tests |currentURLWithTrustLevel:| method.
TEST_P(CRWWebControllerTest, CurrentUrlWithTrustLevel) {
  GURL url("http://chromium.test");
  AddPendingItem(url, ui::PAGE_TRANSITION_TYPED);

  [[[mock_web_view_ stub] andReturnBool:NO] hasOnlySecureContent];
  [static_cast<WKWebView*>([[mock_web_view_ stub] andReturn:@""]) title];
  SetWebViewURL(@"http://chromium.test");

  // Stub out the injection process.
  [[mock_web_view_ stub] evaluateJavaScript:OCMOCK_ANY
                          completionHandler:OCMOCK_ANY];

  // Simulate a page load to trigger a URL update.
  [navigation_delegate_ webView:mock_web_view_
      didStartProvisionalNavigation:nil];
  [fake_wk_list_ setCurrentURL:@"http://chromium.test"];
  [navigation_delegate_ webView:mock_web_view_ didCommitNavigation:nil];

  URLVerificationTrustLevel trust_level = kNone;
  EXPECT_EQ(url, [web_controller() currentURLWithTrustLevel:&trust_level]);
  EXPECT_EQ(kAbsolute, trust_level);
}

INSTANTIATE_TEST_SUITES(CRWWebControllerResponseTest);

// Test fixture to test decidePolicyForNavigationAction:decisionHandler:
// decisionHandler's callback result.
class CRWWebControllerPolicyDeciderTest : public CRWWebControllerTest {
 protected:
  void SetUp() override {
    CRWWebControllerTest::SetUp();
  }
  // Calls webView:decidePolicyForNavigationAction:decisionHandler: callback
  // and waits for decision handler call. Returns false if decision handler
  // policy parameter didn't match |expected_policy| or if the call timed out.
  bool VerifyDecidePolicyForNavigationAction(
      NSURLRequest* request,
      WKNavigationActionPolicy expected_policy) WARN_UNUSED_RESULT {
    CRWFakeWKNavigationAction* navigation_action =
        [[CRWFakeWKNavigationAction alloc] init];
    navigation_action.request = request;

    CRWFakeWKFrameInfo* frame_info = [[CRWFakeWKFrameInfo alloc] init];
    frame_info.mainFrame = YES;
    navigation_action.targetFrame = frame_info;

    // Call decidePolicyForNavigationResponse and wait for decisionHandler's
    // callback.
    __block bool policy_match = false;
    __block bool callback_called = false;
    [navigation_delegate_ webView:mock_web_view_
        decidePolicyForNavigationAction:navigation_action
                        decisionHandler:^(WKNavigationActionPolicy policy) {
                          policy_match = expected_policy == policy;
                          callback_called = true;
                        }];
    callback_called = WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
      return callback_called;
    });
    return policy_match;
  }

  // Return an owned BrowserState in order to set off the record state.
  BrowserState* GetBrowserState() override { return &browser_state_; }

  TestBrowserState browser_state_;
};

// Tests that App specific URLs in iframes are allowed if the main frame is App
// specific URL. App specific pages have elevated privileges and WKWebView uses
// the same renderer process for all page frames. With that running App specific
// pages are not allowed in the same process as a web site from the internet.
TEST_P(CRWWebControllerPolicyDeciderTest,
       AllowAppSpecificIFrameFromAppSpecificPage) {
  NSURL* app_url = [NSURL URLWithString:@(kTestAppSpecificURL)];
  NSMutableURLRequest* app_url_request =
      [NSMutableURLRequest requestWithURL:app_url];
  app_url_request.mainDocumentURL = app_url;
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      app_url_request, WKNavigationActionPolicyAllow));
}

// Tests that URL is allowed in OffTheRecord mode when the
// |kBlockUniversalLinksInOffTheRecordMode| feature is disabled.
TEST_P(CRWWebControllerPolicyDeciderTest, AllowOffTheRecordNavigation) {
  browser_state_.SetOffTheRecord(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      web::features::kBlockUniversalLinksInOffTheRecordMode);

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  url_request.mainDocumentURL = url;
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyAllow));
}

// Tests that URL is allowed in OffTheRecord mode and that universal links are
// blocked when the |kBlockUniversalLinksInOffTheRecordMode| feature is enabled
// and the BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE buildflag is set.
TEST_P(CRWWebControllerPolicyDeciderTest,
       AllowOffTheRecordNavigationBlockUniversalLinks) {
  browser_state_.SetOffTheRecord(true);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      web::features::kBlockUniversalLinksInOffTheRecordMode);

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  url_request.mainDocumentURL = url;

  WKNavigationActionPolicy expected_policy = WKNavigationActionPolicyAllow;
#if BUILDFLAG(BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE)
  expected_policy = kNavigationActionPolicyAllowAndBlockUniversalLinks;
#endif  // BUILDFLAG(BLOCK_UNIVERSAL_LINKS_IN_OFF_THE_RECORD_MODE)

  EXPECT_TRUE(
      VerifyDecidePolicyForNavigationAction(url_request, expected_policy));
}

// Tests that App specific URLs in iframes are not allowed if the main frame is
// not App specific URL.
TEST_P(CRWWebControllerPolicyDeciderTest,
       DisallowAppSpecificIFrameFromRegularPage) {
  NSURL* app_url = [NSURL URLWithString:@(kTestAppSpecificURL)];
  NSMutableURLRequest* app_url_request =
      [NSMutableURLRequest requestWithURL:app_url];
  app_url_request.mainDocumentURL = [NSURL URLWithString:@(kTestURLString)];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      app_url_request, WKNavigationActionPolicyCancel));
}

// Tests that blob URL navigation is allowed.
TEST_P(CRWWebControllerPolicyDeciderTest, BlobUrl) {
  NSURL* blob_url = [NSURL URLWithString:@"blob://aslfkh-asdkjh"];
  NSMutableURLRequest* blob_url_request =
      [NSMutableURLRequest requestWithURL:blob_url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      blob_url_request, WKNavigationActionPolicyAllow));
}

// Tests that navigations which close the WebState cancels the navigation.
// This occurs, for example, when a new page is opened with a link that is
// handled by a native application.
TEST_P(CRWWebControllerPolicyDeciderTest, ClosedWebState) {
  static CRWWebControllerPolicyDeciderTest* test_fixture = nullptr;
  test_fixture = this;
  class FakeWebStateDelegate : public TestWebStateDelegate {
   public:
    void CloseWebState(WebState* source) override {
      test_fixture->DestroyWebState();
    }
  };
  FakeWebStateDelegate delegate;
  web_state()->SetDelegate(&delegate);

  FakeWebStatePolicyDecider policy_decider(web_state());
  policy_decider.SetShouldAllowRequest(false);

  NSURL* url =
      [NSURL URLWithString:@"https://itunes.apple.com/us/album/american-radio/"
                           @"1449089454?fbclid=IwAR2NKLvDGH_YY4uZbU7Cj_"
                           @"e3h7q7DvORCI4Edvi1K9LjcUHfYObmOWl-YgE"];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyCancel));
}

// Tests that navigations are cancelled if the web state is closed in
// |ShouldAllowRequest|.
TEST_P(CRWWebControllerPolicyDeciderTest, ClosedWebStateInShouldAllowRequest) {
  static CRWWebControllerPolicyDeciderTest* test_fixture = nullptr;
  test_fixture = this;

  class TestWebStatePolicyDecider : public WebStatePolicyDecider {
   public:
    explicit TestWebStatePolicyDecider(WebState* web_state)
        : WebStatePolicyDecider(web_state) {}
    ~TestWebStatePolicyDecider() override = default;

    // WebStatePolicyDecider overrides
    bool ShouldAllowRequest(NSURLRequest* request,
                            const RequestInfo& request_info) override {
      test_fixture->DestroyWebState();
      return true;
    }
    bool ShouldAllowResponse(NSURLResponse* response,
                             bool for_main_frame) override {
      return true;
    }
    void WebStateDestroyed() override {}
  };
  TestWebStatePolicyDecider policy_decider(web_state());

  NSURL* url = [NSURL URLWithString:@(kTestURLString)];
  NSMutableURLRequest* url_request = [NSMutableURLRequest requestWithURL:url];
  EXPECT_TRUE(VerifyDecidePolicyForNavigationAction(
      url_request, WKNavigationActionPolicyCancel));
}

INSTANTIATE_TEST_SUITES(CRWWebControllerPolicyDeciderTest);

// Test fixture for testing CRWWebController presenting native content.
class CRWWebControllerNativeContentTest
    : public ProgrammaticWebTestWithWebController {
 protected:
  void SetUp() override {
    ProgrammaticWebTestWithWebController::SetUp();
    mock_native_provider_ = [[TestNativeContentProvider alloc] init];
    [[web_controller() nativeContentHolder]
        setNativeProvider:mock_native_provider_];
  }

  void Load(const GURL& URL) {
    TestWebStateObserver observer(web_state());
    NavigationManagerImpl& navigation_manager =
        [web_controller() webStateImpl]->GetNavigationManagerImpl();
    navigation_manager.AddPendingItem(
        URL, Referrer(), ui::PAGE_TRANSITION_TYPED,
        NavigationInitiationType::BROWSER_INITIATED,
        NavigationManager::UserAgentOverrideOption::INHERIT);
    [web_controller() loadCurrentURLWithRendererInitiatedNavigation:NO];

    // Native URL is loaded asynchronously with WKBasedNavigationManager. Wait
    // for navigation to finish before asserting.
    if (GetWebClient()->IsSlimNavigationManagerEnabled()) {
      TestWebStateObserver* observer_ptr = &observer;
      ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
        return observer_ptr->did_finish_navigation_info() != nullptr;
      }));
    }
  }

  TestNativeContentProvider* mock_native_provider_;
};

// Tests WebState and NavigationManager correctly return native content URL.
TEST_P(CRWWebControllerNativeContentTest, NativeContentURL) {
  GURL url_to_load(kTestAppSpecificURL);
  TestNativeContent* content =
      [[TestNativeContent alloc] initWithURL:url_to_load virtualURL:GURL()];
  [mock_native_provider_ setController:content forURL:url_to_load];
  Load(url_to_load);
  URLVerificationTrustLevel trust_level = kNone;
  GURL gurl = [web_controller() currentURLWithTrustLevel:&trust_level];
  EXPECT_EQ(gurl, url_to_load);
  EXPECT_EQ(kAbsolute, trust_level);
  EXPECT_EQ([web_controller() webState]->GetVisibleURL(), url_to_load);
  NavigationManagerImpl& navigationManager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetVirtualURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetVirtualURL(),
            url_to_load);
}

// Tests WebState and NavigationManager correctly return native content URL and
// VirtualURL
TEST_P(CRWWebControllerNativeContentTest, NativeContentVirtualURL) {
  GURL url_to_load(kTestAppSpecificURL);
  GURL virtual_url(kTestURLString);
  TestNativeContent* content =
      [[TestNativeContent alloc] initWithURL:virtual_url
                                  virtualURL:virtual_url];
  [mock_native_provider_ setController:content forURL:url_to_load];
  Load(url_to_load);
  URLVerificationTrustLevel trust_level = kNone;
  GURL gurl = [web_controller() currentURLWithTrustLevel:&trust_level];
  EXPECT_EQ(gurl, virtual_url);
  EXPECT_EQ(kAbsolute, trust_level);
  EXPECT_EQ([web_controller() webState]->GetVisibleURL(), virtual_url);
  NavigationManagerImpl& navigationManager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetVisibleItem()->GetVirtualURL(), virtual_url);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetURL(), url_to_load);
  EXPECT_EQ(navigationManager.GetLastCommittedItem()->GetVirtualURL(),
            virtual_url);
}

INSTANTIATE_TEST_SUITES(CRWWebControllerNativeContentTest);

// Test fixture for window.open tests.
class WindowOpenByDomTest : public ProgrammaticWebTestWithWebController {
 protected:
  WindowOpenByDomTest() : opener_url_("http://test") {}

  void SetUp() override {
    ProgrammaticWebTestWithWebController::SetUp();
    web_state()->SetDelegate(&delegate_);
    LoadHtml(@"<html><body></body></html>", opener_url_);
  }
  // Executes JavaScript that opens a new window and returns evaluation result
  // as a string.
  id OpenWindowByDom() {
    NSString* const kOpenWindowScript =
        @"w = window.open('javascript:void(0);', target='_blank');"
         "w ? w.toString() : null;";
    id windowJSObject = ExecuteJavaScript(kOpenWindowScript);
    return windowJSObject;
  }

  // Executes JavaScript that closes previously opened window.
  void CloseWindow() { ExecuteJavaScript(@"w.close()"); }

  // URL of a page which opens child windows.
  const GURL opener_url_;
  TestWebStateDelegate delegate_;
};

// Tests that absence of web state delegate is handled gracefully.
TEST_P(WindowOpenByDomTest, NoDelegate) {
  web_state()->SetDelegate(nullptr);

  EXPECT_NSEQ([NSNull null], OpenWindowByDom());

  EXPECT_TRUE(delegate_.child_windows().empty());
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.open triggered by user gesture opens a new non-popup
// window.
TEST_P(WindowOpenByDomTest, OpenWithUserGesture) {
  [web_controller() touched:YES];
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.open executed w/o user gesture does not open a new window,
// but blocks popup instead.
TEST_P(WindowOpenByDomTest, BlockPopup) {
  EXPECT_NSEQ([NSNull null], OpenWindowByDom());

  EXPECT_TRUE(delegate_.child_windows().empty());
  ASSERT_EQ(1U, delegate_.popups().size());
  EXPECT_EQ(GURL("javascript:void(0);"), delegate_.popups()[0].url);
  EXPECT_EQ(opener_url_, delegate_.popups()[0].opener_url);
}

// Tests that window.open executed w/o user gesture opens a new window, assuming
// that delegate allows popups.
TEST_P(WindowOpenByDomTest, DontBlockPopup) {
  delegate_.allow_popups(opener_url_);
  EXPECT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());
}

// Tests that window.close closes the web state.
TEST_P(WindowOpenByDomTest, CloseWindow) {
  delegate_.allow_popups(opener_url_);
  ASSERT_NSEQ(@"[object Window]", OpenWindowByDom());

  ASSERT_EQ(1U, delegate_.child_windows().size());
  ASSERT_TRUE(delegate_.child_windows()[0]);
  EXPECT_TRUE(delegate_.popups().empty());

  delegate_.child_windows()[0]->SetDelegate(&delegate_);
  CloseWindow();

  EXPECT_TRUE(delegate_.child_windows().empty());
  EXPECT_TRUE(delegate_.popups().empty());
}

INSTANTIATE_TEST_SUITES(WindowOpenByDomTest);

// Tests page title changes.
typedef ProgrammaticWebTestWithWebState CRWWebControllerTitleTest;
TEST_P(CRWWebControllerTitleTest, TitleChange) {
  // Observes and waits for TitleWasSet call.
  class TitleObserver : public WebStateObserver {
   public:
    TitleObserver() = default;

    // Returns number of times |TitleWasSet| was called.
    int title_change_count() { return title_change_count_; }
    // WebStateObserver overrides:
    void TitleWasSet(WebState* web_state) override { title_change_count_++; }
    void WebStateDestroyed(WebState* web_state) override { NOTREACHED(); }

   private:
    int title_change_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(TitleObserver);
  };

  TitleObserver observer;
  ScopedObserver<WebState, WebStateObserver> scoped_observer(&observer);
  scoped_observer.Add(web_state());
  ASSERT_EQ(0, observer.title_change_count());

  // Expect TitleWasSet callback after the page is loaded and due to WKWebView
  // title change KVO.
  LoadHtml(@"<title>Title1</title>");
  EXPECT_EQ("Title1", base::UTF16ToUTF8(web_state()->GetTitle()));
  EXPECT_EQ(2, observer.title_change_count());

  // Expect at least one more TitleWasSet callback after changing title via
  // JavaScript. On iOS 10 WKWebView fires 3 callbacks after JS excucution
  // with the following title changes: "Title2", "" and "Title2".
  // TODO(crbug.com/696104): There should be only 2 calls of TitleWasSet.
  // Fix expecteation when WKWebView stops sending extra KVO calls.
  ExecuteJavaScript(@"window.document.title = 'Title2';");
  EXPECT_EQ("Title2", base::UTF16ToUTF8(web_state()->GetTitle()));
  EXPECT_GE(observer.title_change_count(), 3);
}

// Tests that fragment change navigations use title from the previous page.
TEST_P(CRWWebControllerTitleTest, FragmentChangeNavigationsUsePreviousTitle) {
  LoadHtml(@"<title>Title1</title>");
  ASSERT_EQ("Title1", base::UTF16ToUTF8(web_state()->GetTitle()));
  ExecuteJavaScript(@"window.location.hash = '#1'");
  EXPECT_EQ("Title1", base::UTF16ToUTF8(web_state()->GetTitle()));
}

INSTANTIATE_TEST_SUITES(CRWWebControllerTitleTest);

// Test fixture for JavaScript execution.
class ScriptExecutionTest : public ProgrammaticWebTestWithWebController {
 protected:
  // Calls |executeUserJavaScript:completionHandler:|, waits for script
  // execution completion, and synchronously returns the result.
  id ExecuteUserJavaScript(NSString* java_script, NSError** error) {
    __block id script_result = nil;
    __block NSError* script_error = nil;
    __block bool script_executed = false;
    [web_controller().jsInjector
        executeUserJavaScript:java_script
            completionHandler:^(id local_result, NSError* local_error) {
              script_result = local_result;
              script_error = local_error;
              script_executed = true;
            }];

    WaitForCondition(^{
      return script_executed;
    });

    if (error) {
      *error = script_error;
    }
    return script_result;
  }
};

// Tests evaluating user script on an http page.
TEST_P(ScriptExecutionTest, UserScriptOnHttpPage) {
  LoadHtml(@"<html></html>", GURL(kTestURLString));
  NSError* error = nil;
  EXPECT_NSEQ(@0, ExecuteUserJavaScript(@"window.w = 0;", &error));
  EXPECT_FALSE(error);

  EXPECT_NSEQ(@0, ExecuteJavaScript(@"window.w"));
}

// Tests evaluating user script on app-specific page. Pages with app-specific
// URLs have elevated privileges and JavaScript execution should not be allowed
// for them.
TEST_P(ScriptExecutionTest, UserScriptOnAppSpecificPage) {
  LoadHtml(@"<html></html>", GURL(kTestURLString));

  // Change last committed URL to app-specific URL.
  NavigationManagerImpl& nav_manager =
      [web_controller() webStateImpl]->GetNavigationManagerImpl();
  nav_manager.AddPendingItem(
      GURL(kTestAppSpecificURL), Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::INHERIT);
  nav_manager.CommitPendingItem();

  NSError* error = nil;
  EXPECT_FALSE(ExecuteUserJavaScript(@"window.w = 0;", &error));
  ASSERT_TRUE(error);
  EXPECT_NSEQ(kJSEvaluationErrorDomain, error.domain);
  EXPECT_EQ(JS_EVALUATION_ERROR_CODE_REJECTED, error.code);

  EXPECT_FALSE(ExecuteJavaScript(@"window.w"));
}

INSTANTIATE_TEST_SUITES(ScriptExecutionTest);

// Fixture class to test WKWebView crashes.
class CRWWebControllerWebProcessTest
    : public ProgrammaticWebTestWithWebController {
 protected:
  void SetUp() override {
    ProgrammaticWebTestWithWebController::SetUp();
    webView_ = BuildTerminatedWKWebView();
    TestWebViewContentView* webViewContentView = [[TestWebViewContentView alloc]
        initWithMockWebView:webView_
                 scrollView:[webView_ scrollView]];
    [web_controller() injectWebViewContentView:webViewContentView];

    // This test intentionally crashes the render process.
    SetIgnoreRenderProcessCrashesDuringTesting(true);
  }
  WKWebView* webView_;
};

// Tests that WebStateDelegate::RenderProcessGone is called when WKWebView web
// process has crashed.
TEST_P(CRWWebControllerWebProcessTest, Crash) {
  ASSERT_TRUE([web_controller() isViewAlive]);
  ASSERT_FALSE([web_controller() isWebProcessCrashed]);
  ASSERT_FALSE(web_state()->IsCrashed());
  ASSERT_FALSE(web_state()->IsEvicted());

  TestWebStateObserver observer(web_state());
  TestWebStateObserver* observer_ptr = &observer;
  SimulateWKWebViewCrash(webView_);
  base::test::ios::WaitUntilCondition(^bool() {
    return observer_ptr->render_process_gone_info();
  });
  EXPECT_EQ(web_state(), observer.render_process_gone_info()->web_state);
  EXPECT_FALSE([web_controller() isViewAlive]);
  EXPECT_TRUE([web_controller() isWebProcessCrashed]);
  EXPECT_TRUE(web_state()->IsCrashed());
  EXPECT_TRUE(web_state()->IsEvicted());
}

// Tests that WebState is considered as evicted but not crashed when calling
// SetWebUsageEnabled(false).
TEST_P(CRWWebControllerWebProcessTest, Eviction) {
  ASSERT_TRUE([web_controller() isViewAlive]);
  ASSERT_FALSE([web_controller() isWebProcessCrashed]);
  ASSERT_FALSE(web_state()->IsCrashed());
  ASSERT_FALSE(web_state()->IsEvicted());

  web_state()->SetWebUsageEnabled(false);
  EXPECT_FALSE([web_controller() isViewAlive]);
  EXPECT_FALSE([web_controller() isWebProcessCrashed]);
  EXPECT_FALSE(web_state()->IsCrashed());
  EXPECT_TRUE(web_state()->IsEvicted());
}

INSTANTIATE_TEST_SUITES(CRWWebControllerWebProcessTest);

#undef INSTANTIATE_TEST_SUITES

}  // namespace web
