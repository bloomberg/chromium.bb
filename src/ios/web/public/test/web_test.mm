// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/web_test.h"

#include "base/check.h"
#include "base/memory/ptr_util.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#include "ios/web/public/deprecated/global_web_state_observer.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

class WebTestRenderProcessCrashObserver : public GlobalWebStateObserver {
 public:
  WebTestRenderProcessCrashObserver() = default;
  ~WebTestRenderProcessCrashObserver() override = default;

  void RenderProcessGone(WebState* web_state) override {
    FAIL() << "Renderer process died unexpectedly during the test";
  }
};

WebTest::WebTest(WebTaskEnvironment::Options options)
    : WebTest(std::make_unique<FakeWebClient>(), options) {}

WebTest::WebTest(std::unique_ptr<web::WebClient> web_client,
                 WebTaskEnvironment::Options options)
    : web_client_(std::move(web_client)),
      task_environment_(options),
      crash_observer_(std::make_unique<WebTestRenderProcessCrashObserver>()) {}

WebTest::~WebTest() {}

void WebTest::SetUp() {
  PlatformTest::SetUp();

  DCHECK(!browser_state_);
  browser_state_ = CreateBrowserState();
  DCHECK(browser_state_);
}

std::unique_ptr<BrowserState> WebTest::CreateBrowserState() {
  return std::make_unique<FakeBrowserState>();
}

void WebTest::OverrideJavaScriptFeatures(
    std::vector<JavaScriptFeature*> features) {
  WKWebViewConfigurationProvider& configuration_provider =
      WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
  WKWebViewConfiguration* configuration =
      configuration_provider.GetWebViewConfiguration();
  // User scripts must be removed because
  // |JavaScriptFeatureManager::ConfigureFeatures| will remove script message
  // handlers.
  [configuration.userContentController removeAllUserScripts];

  JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
      ->ConfigureFeatures(features);
}

web::WebClient* WebTest::GetWebClient() {
  return web_client_.Get();
}

BrowserState* WebTest::GetBrowserState() {
  DCHECK(browser_state_);
  return browser_state_.get();
}

void WebTest::SetIgnoreRenderProcessCrashesDuringTesting(bool allow) {
  if (allow) {
    crash_observer_ = nullptr;
  } else {
    crash_observer_ = std::make_unique<WebTestRenderProcessCrashObserver>();
  }
}

}  // namespace web
