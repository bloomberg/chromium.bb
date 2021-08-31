// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/java_script_feature_manager.h"

#import <WebKit/WebKit.h>

#include "base/ios/ios_util.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A test fixture for testing JavaScriptFeatureManager.
class JavaScriptFeatureManagerTest : public web::WebTest {
 protected:
  JavaScriptFeatureManagerTest()
      : web::WebTest(std::make_unique<web::FakeWebClient>()) {}

  web::JavaScriptFeatureManager* GetJavaScriptFeatureManager() {
    web::JavaScriptFeatureManager* java_script_feature_manager =
        web::JavaScriptFeatureManager::FromBrowserState(GetBrowserState());
    return java_script_feature_manager;
  }

  WKUserContentController* GetUserContentController() {
    return web::WKWebViewConfigurationProvider::FromBrowserState(
               GetBrowserState())
        .GetWebViewConfiguration()
        .userContentController;
  }

  void SetUp() override {
    web::WebTest::SetUp();
    [GetUserContentController() removeAllUserScripts];
  }
};

// Tests that JavaScriptFeatureManager adds base shared user scripts.
TEST_F(JavaScriptFeatureManagerTest, Configure) {
  ASSERT_TRUE(GetJavaScriptFeatureManager());
  ASSERT_EQ(0ul, [GetUserContentController().userScripts count]);

  GetJavaScriptFeatureManager()->ConfigureFeatures({});
  if (base::ios::IsRunningOnIOS14OrLater()) {
    EXPECT_EQ(6ul, [GetUserContentController().userScripts count]);
  } else {
    EXPECT_EQ(3ul, [GetUserContentController().userScripts count]);
  }
}

// Tests that JavaScriptFeatureManager adds a JavaScriptFeature for all frames
// at document start time for the page content world.
TEST_F(JavaScriptFeatureManagerTest, AllFramesStartFeature) {
  ASSERT_TRUE(GetJavaScriptFeatureManager());

  std::vector<const web::JavaScriptFeature::FeatureScript> feature_scripts = {
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "java_script_feature_test_inject_once_js",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentStart,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kAllFrames)};

  std::unique_ptr<web::JavaScriptFeature> feature =
      std::make_unique<web::JavaScriptFeature>(
          web::JavaScriptFeature::ContentWorld::kPageContentWorld,
          feature_scripts);

  GetJavaScriptFeatureManager()->ConfigureFeatures({feature.get()});

  if (base::ios::IsRunningOnIOS14OrLater()) {
    EXPECT_EQ(7ul, [GetUserContentController().userScripts count]);
  } else {
    EXPECT_EQ(4ul, [GetUserContentController().userScripts count]);
  }
  WKUserScript* user_script =
      [GetUserContentController().userScripts lastObject];
  EXPECT_TRUE(
      [user_script.source containsString:@"__gCrWeb.javaScriptFeatureTest"]);
  EXPECT_EQ(WKUserScriptInjectionTimeAtDocumentStart,
            user_script.injectionTime);
  EXPECT_EQ(NO, user_script.forMainFrameOnly);
}

// Tests that JavaScriptFeatureManager adds a JavaScriptFeature for all frames
// at document end time for any content world.
TEST_F(JavaScriptFeatureManagerTest, MainFrameEndFeature) {
  ASSERT_TRUE(GetJavaScriptFeatureManager());

  std::vector<const web::JavaScriptFeature::FeatureScript> feature_scripts = {
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "java_script_feature_test_inject_once_js",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame)};

  std::unique_ptr<web::JavaScriptFeature> feature =
      std::make_unique<web::JavaScriptFeature>(
          web::JavaScriptFeature::ContentWorld::kAnyContentWorld,
          feature_scripts);

  GetJavaScriptFeatureManager()->ConfigureFeatures({feature.get()});

  if (base::ios::IsRunningOnIOS14OrLater()) {
    EXPECT_EQ(7ul, [GetUserContentController().userScripts count]);
  } else {
    EXPECT_EQ(4ul, [GetUserContentController().userScripts count]);
  }
  WKUserScript* user_script =
      [GetUserContentController().userScripts lastObject];
  EXPECT_TRUE(
      [user_script.source containsString:@"__gCrWeb.javaScriptFeatureTest"]);
  EXPECT_EQ(WKUserScriptInjectionTimeAtDocumentEnd, user_script.injectionTime);
  EXPECT_EQ(YES, user_script.forMainFrameOnly);
}

// Tests that JavaScriptFeatureManager adds a JavaScriptFeature for all frames
// at document end time for an isolated world.
TEST_F(JavaScriptFeatureManagerTest, MainFrameEndFeatureIsolatedWorld) {
  // Using ContentWorld::kIsolatedWorldOnly on older versions of iOS will
  // trigger a DCHECK, so return early before that happens.
  if (!base::ios::IsRunningOnIOS14OrLater()) {
    return;
  }

  ASSERT_TRUE(GetJavaScriptFeatureManager());

  std::vector<const web::JavaScriptFeature::FeatureScript> feature_scripts = {
      web::JavaScriptFeature::FeatureScript::CreateWithFilename(
          "java_script_feature_test_inject_once_js",
          web::JavaScriptFeature::FeatureScript::InjectionTime::kDocumentEnd,
          web::JavaScriptFeature::FeatureScript::TargetFrames::kMainFrame)};

  std::unique_ptr<web::JavaScriptFeature> feature =
      std::make_unique<web::JavaScriptFeature>(
          web::JavaScriptFeature::ContentWorld::kIsolatedWorldOnly,
          feature_scripts);

  GetJavaScriptFeatureManager()->ConfigureFeatures({feature.get()});

  EXPECT_EQ(7ul, [GetUserContentController().userScripts count]);
  WKUserScript* user_script =
      [GetUserContentController().userScripts lastObject];
  EXPECT_TRUE(
      [user_script.source containsString:@"__gCrWeb.javaScriptFeatureTest"]);
  EXPECT_EQ(WKUserScriptInjectionTimeAtDocumentEnd, user_script.injectionTime);
  EXPECT_EQ(YES, user_script.forMainFrameOnly);
}
