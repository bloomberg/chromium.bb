// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_manager.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/test/fakes/fake_java_script_feature.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForPageLoadTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

typedef WebTestWithWebState JavaScriptFeatureManagerIntTest;

// Tests that a JavaScriptFeature added by JavaScriptFeatureManager to the page
// content world correctly receives script message callbacks.
TEST_F(JavaScriptFeatureManagerIntTest, AddFeatureToPageContentWorld) {
  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kPageContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  ASSERT_TRUE(LoadHtml("<html></html>"));

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

TEST_F(JavaScriptFeatureManagerIntTest,
       PageContentWorldIFrameScriptMessageHandler) {
  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kPageContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  ASSERT_TRUE(LoadHtml("<html><iframe></iframe></html>"));

  ASSERT_FALSE(feature.last_received_browser_state());
  ASSERT_FALSE(feature.last_received_message());

  __block std::set<WebFrame*> web_frames;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    web_frames = web_state()->GetWebFramesManager()->GetAllWebFrames();
    return web_frames.size() == 2;
  }));

  WebFrame* child_frame = nullptr;
  for (WebFrame* frame : web_frames) {
    if (frame != GetMainFrame(web_state())) {
      child_frame = frame;
      break;
    }
  }

  ASSERT_TRUE(child_frame);

  std::vector<base::Value> parameters;
  parameters.push_back(
      base::Value(kFakeJavaScriptFeaturePostMessageReplyValue));
  feature.ReplyWithPostMessage(child_frame, parameters);

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

TEST_F(JavaScriptFeatureManagerIntTest, AddFeatureToIsolatedWorld) {
  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  ASSERT_TRUE(LoadHtml("<html></html>"));

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

TEST_F(JavaScriptFeatureManagerIntTest,
       IsolatedWorldIFrameScriptMessageHandler) {
  FakeJavaScriptFeature feature(
      JavaScriptFeature::ContentWorld::kAnyContentWorld);

  web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures({&feature});

  ASSERT_TRUE(LoadHtml("<html><iframe></iframe></html>"));

  ASSERT_FALSE(feature.last_received_browser_state());
  ASSERT_FALSE(feature.last_received_message());

  __block std::set<WebFrame*> web_frames;
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    web_frames = web_state()->GetWebFramesManager()->GetAllWebFrames();
    return web_frames.size() == 2;
  }));

  WebFrame* child_frame = nullptr;
  for (WebFrame* frame : web_frames) {
    if (frame != GetMainFrame(web_state())) {
      child_frame = frame;
      break;
    }
  }

  ASSERT_TRUE(child_frame);

  std::vector<base::Value> parameters;
  parameters.push_back(
      base::Value(kFakeJavaScriptFeaturePostMessageReplyValue));
  feature.ReplyWithPostMessage(child_frame, parameters);

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

}  // namespace web
