// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/ios/wait_util.h"

#include "base/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#include "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/test/fakes/fake_java_script_feature.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

static NSString* kPageHTML =
    @"<html><body>"
     "  <div id=\"div\">contents1</div><div id=\"div2\">contents2</div>"
     "</body></html>";

namespace web {

typedef WebTestWithWebState JavaScriptFeatureTest;

// Tests that a JavaScriptFeature executes its injected JavaScript when
// configured in the page content world.
TEST_F(JavaScriptFeatureTest, JavaScriptFeatureInjectJavaScript) {
  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kPageContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText));
}

// Tests that a JavaScriptFeature executes its injected JavaScript when
// configured in an isolated world.
TEST_F(JavaScriptFeatureTest, JavaScriptFeatureInjectJavaScriptIsolatedWorld) {
  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kAnyContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(
      web_state(), kFakeJavaScriptFeatureLoadedText));
}

// Tests that a JavaScriptFeature correctly calls JavaScript functions when
// configured in the page content world.
TEST_F(JavaScriptFeatureTest, JavaScriptFeatureExecuteJavaScript) {
  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kPageContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));

  feature.ReplaceDivContents(GetMainFrame(web_state()));

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));
}

// Tests that a JavaScriptFeature correctly calls JavaScript functions when
// configured in an isolated world.
TEST_F(JavaScriptFeatureTest,
       JavaScriptFeatureExecuteJavaScriptInIsolatedWorld) {
  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kAnyContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));

  feature.ReplaceDivContents(GetMainFrame(web_state()));

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));
}

// Tests that a JavaScriptFeature correctly calls JavaScript functions when
// configured in an isolated world only.
TEST_F(JavaScriptFeatureTest,
       JavaScriptFeatureExecuteJavaScriptInIsolatedWorldOnly) {
  // Using ContentWorld::kIsolatedWorldOnly on older versions of iOS will
  // trigger a DCHECK, so return early.
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }

  FakeJavaScriptFeature feature(
      web::JavaScriptFeature::ContentWorld::kIsolatedWorldOnly);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents1"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));

  feature.ReplaceDivContents(GetMainFrame(web_state()));

  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "updated"));
  EXPECT_TRUE(test::WaitForWebViewContainingText(web_state(), "contents2"));
}

// Tests that a JavaScriptFeature receives post messages from JavaScript for
// registered names in the page content world.
TEST_F(JavaScriptFeatureTest, MessageHandlerInPageContentWorld) {
  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kPageContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature.last_received_browser_state());
  ASSERT_FALSE(feature.last_received_message());

  std::vector<base::Value> parameters;
  parameters.push_back(
      base::Value(kFakeJavaScriptFeaturePostMessageReplyValue));
  feature.ReplyWithPostMessage(GetMainFrame(web_state()), parameters);

  FakeJavaScriptFeature* feature_ptr = &feature;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature_ptr->last_received_browser_state();
  }));

  EXPECT_EQ(GetBrowserState(), feature.last_received_browser_state());

  ASSERT_TRUE(feature.last_received_message());
  EXPECT_EQ(kFakeJavaScriptFeatureScriptHandlerName,
            base::SysNSStringToUTF8(feature.last_received_message().name));
  EXPECT_EQ(kFakeJavaScriptFeaturePostMessageReplyValue,
            base::SysNSStringToUTF8(feature.last_received_message().body));
}

// Tests that a JavaScriptFeature receives post messages from JavaScript for
// registered names in an isolated world.
TEST_F(JavaScriptFeatureTest, MessageHandlerInIsolatedWorld) {
  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature.last_received_browser_state());
  ASSERT_FALSE(feature.last_received_message());

  std::vector<base::Value> parameters;
  parameters.push_back(
      base::Value(kFakeJavaScriptFeaturePostMessageReplyValue));
  feature.ReplyWithPostMessage(GetMainFrame(web_state()), parameters);

  FakeJavaScriptFeature* feature_ptr = &feature;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return feature_ptr->last_received_browser_state();
  }));

  EXPECT_EQ(GetBrowserState(), feature.last_received_browser_state());

  ASSERT_TRUE(feature.last_received_message());
  EXPECT_EQ(kFakeJavaScriptFeatureScriptHandlerName,
            base::SysNSStringToUTF8(feature.last_received_message().name));
  EXPECT_EQ(kFakeJavaScriptFeaturePostMessageReplyValue,
            base::SysNSStringToUTF8(feature.last_received_message().body));
}

// Tests that a JavaScriptFeature with
// ReinjectionBehavior::kReinjectOnDocumentRecreation re-injects JavaScript in
// the page content world.
TEST_F(JavaScriptFeatureTest, ReinjectionBehaviorPageContentWorld) {
  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kPageContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature.last_received_browser_state());
  ASSERT_FALSE(feature.last_received_message());

  __block bool count_received = false;
  feature.GetErrorCount(GetMainFrame(web_state()),
                        base::BindOnce(^void(const base::Value* count) {
                          ASSERT_TRUE(count);
                          ASSERT_TRUE(count->is_double());
                          ASSERT_EQ(0ul, count->GetDouble());
                          count_received = true;
                        }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));

  ExecuteJavaScript(@"invalidFunction();");

  count_received = false;
  feature.GetErrorCount(GetMainFrame(web_state()),
                        base::BindOnce(^void(const base::Value* count) {
                          ASSERT_TRUE(count);
                          ASSERT_TRUE(count->is_double());
                          ASSERT_EQ(1ul, count->GetDouble());
                          count_received = true;
                        }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));

  ASSERT_TRUE(ExecuteJavaScript(
      @"document.open(); document.write('<p></p>'); document.close(); true;"));

  ExecuteJavaScript(@"invalidFunction();");

  count_received = false;
  feature.GetErrorCount(GetMainFrame(web_state()),
                        base::BindOnce(^void(const base::Value* count) {
                          ASSERT_TRUE(count);
                          ASSERT_TRUE(count->is_double());
                          EXPECT_EQ(2ul, count->GetDouble());
                          count_received = true;
                        }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));
}

// Tests that a JavaScriptFeature with
// ReinjectionBehavior::kReinjectOnDocumentRecreation re-injects JavaScript in
// an isolated world.
TEST_F(JavaScriptFeatureTest, ReinjectionBehaviorIsolatedWorld) {
  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  LoadHtml(kPageHTML);

  ASSERT_FALSE(feature.last_received_browser_state());
  ASSERT_FALSE(feature.last_received_message());

  __block bool count_received = false;
  feature.GetErrorCount(GetMainFrame(web_state()),
                        base::BindOnce(^void(const base::Value* count) {
                          ASSERT_TRUE(count);
                          ASSERT_TRUE(count->is_double());
                          ASSERT_EQ(0ul, count->GetDouble());
                          count_received = true;
                        }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));

  ExecuteJavaScript(@"invalidFunction();");

  count_received = false;
  feature.GetErrorCount(GetMainFrame(web_state()),
                        base::BindOnce(^void(const base::Value* count) {
                          ASSERT_TRUE(count);
                          ASSERT_TRUE(count->is_double());
                          ASSERT_EQ(1ul, count->GetDouble());
                          count_received = true;
                        }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));

  ASSERT_TRUE(ExecuteJavaScript(
      @"document.open(); document.write('<p></p>'); document.close(); true;"));

  ExecuteJavaScript(@"invalidFunction();");

  count_received = false;
  feature.GetErrorCount(GetMainFrame(web_state()),
                        base::BindOnce(^void(const base::Value* count) {
                          ASSERT_TRUE(count);
                          ASSERT_TRUE(count->is_double());
                          EXPECT_EQ(2ul, count->GetDouble());
                          count_received = true;
                        }));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return count_received;
  }));
}

}  // namespace web
