// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/page_script_util.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/common/web_view_creation_util.h"
#include "ios/web/public/browsing_data/cookie_blocking_mode.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#include "ios/web/public/test/web_test.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {
namespace {

// A test fixture for testing the page_script_util methods.
class PageScriptUtilTest : public WebTest {
 protected:
  PageScriptUtilTest() : WebTest(std::make_unique<TestWebClient>()) {}

  TestWebClient* GetWebClient() override {
    return static_cast<TestWebClient*>(WebTest::GetWebClient());
  }
};

// Tests that WKWebView early page script is a valid script that injects global
// __gCrWeb object.
TEST_F(PageScriptUtilTest, WKWebViewEarlyPageScript) {
  WKWebView* web_view = BuildWKWebView(CGRectZero, GetBrowserState());
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForAllFrames(GetBrowserState()));
  EXPECT_NSEQ(@"object", test::ExecuteJavaScript(web_view, @"typeof __gCrWeb"));
}

// Tests that embedder's WKWebView script is included into early script.
TEST_F(PageScriptUtilTest, WKEmbedderScript) {
  GetWebClient()->SetEarlyPageScript(@"__gCrEmbedder = {};");
  WKWebView* web_view = BuildWKWebView(CGRectZero, GetBrowserState());
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForAllFrames(GetBrowserState()));
  test::ExecuteJavaScript(
      web_view, GetDocumentStartScriptForMainFrame(GetBrowserState()));
  EXPECT_NSEQ(@"object",
              test::ExecuteJavaScript(web_view, @"typeof __gCrEmbedder"));
}

// Tests that the correct replacement has been made for the cookie blocking
// state in the DocumentStartScriptForAllFrames.
TEST_F(PageScriptUtilTest, AllFrameStartCookieReplacement) {
  web::BrowserState* browser_state = GetBrowserState();

  __block bool success = false;
  browser_state->SetCookieBlockingMode(web::CookieBlockingMode::kAllow,
                                       base::BindOnce(^{
                                         success = true;
                                       }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));

  NSString* script = GetDocumentStartScriptForAllFrames(browser_state);
  EXPECT_EQ(0U, [script rangeOfString:@"$(COOKIE_STATE)"].length);
  EXPECT_LT(0U, [script rangeOfString:@"(\"allow\")"].length);

  success = false;
  browser_state->SetCookieBlockingMode(
      web::CookieBlockingMode::kBlockThirdParty, base::BindOnce(^{
        success = true;
      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));
  script = GetDocumentStartScriptForAllFrames(browser_state);
  EXPECT_EQ(0U, [script rangeOfString:@"$(COOKIE_STATE)"].length);
  EXPECT_LT(0U, [script rangeOfString:@"(\"block-third-party\")"].length);

  success = false;
  browser_state->SetCookieBlockingMode(web::CookieBlockingMode::kBlock,
                                       base::BindOnce(^{
                                         success = true;
                                       }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return success;
  }));
  script = GetDocumentStartScriptForAllFrames(browser_state);
  EXPECT_EQ(0U, [script rangeOfString:@"$(COOKIE_STATE)"].length);
  EXPECT_LT(0U, [script rangeOfString:@"(\"block\")"].length);
}

}  // namespace
}  // namespace web
