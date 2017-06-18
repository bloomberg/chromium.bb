// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebHelperPlugin.h"

#include "core/frame/FrameTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebLocalFrame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/tests/FakeWebPlugin.h"

namespace blink {

namespace {

class FakePlaceholderWebPlugin : public FakeWebPlugin {
 public:
  explicit FakePlaceholderWebPlugin(const WebPluginParams& params)
      : FakeWebPlugin(params) {}
  ~FakePlaceholderWebPlugin() override {}

  bool IsPlaceholder() override { return true; }
};

class WebHelperPluginFrameClient : public FrameTestHelpers::TestWebFrameClient {
 public:
  WebHelperPluginFrameClient() : create_placeholder_(false) {}
  ~WebHelperPluginFrameClient() override {}

  WebPlugin* CreatePlugin(const WebPluginParams& params) override {
    return create_placeholder_ ? new FakePlaceholderWebPlugin(params)
                               : new FakeWebPlugin(params);
  }

  void SetCreatePlaceholder(bool create_placeholder) {
    create_placeholder_ = create_placeholder;
  }

 private:
  bool create_placeholder_;
};

class WebHelperPluginTest : public ::testing::Test {
 protected:
  void SetUp() override {
    helper_.InitializeAndLoad("about:blank", &frame_client_);
  }

  void DestroyHelperPlugin() {
    plugin_.reset();
    // WebHelperPlugin is destroyed by a task posted to the message loop.
    testing::RunPendingTasks();
  }

  WebHelperPluginFrameClient frame_client_;
  FrameTestHelpers::WebViewHelper helper_;
  WebHelperPluginUniquePtr plugin_;
};

TEST_F(WebHelperPluginTest, CreateAndDestroyAfterWebViewDestruction) {
  plugin_.reset(WebHelperPlugin::Create(
      "hello", helper_.WebView()->MainFrame()->ToWebLocalFrame()));
  EXPECT_TRUE(plugin_);
  EXPECT_TRUE(plugin_->GetPlugin());

  DestroyHelperPlugin();
}

TEST_F(WebHelperPluginTest, CreateAndDestroyBeforeWebViewDestruction) {
  plugin_.reset(WebHelperPlugin::Create(
      "hello", helper_.WebView()->MainFrame()->ToWebLocalFrame()));
  EXPECT_TRUE(plugin_);
  EXPECT_TRUE(plugin_->GetPlugin());

  DestroyHelperPlugin();
}

TEST_F(WebHelperPluginTest, CreateFailsWithPlaceholder) {
  frame_client_.SetCreatePlaceholder(true);

  plugin_.reset(WebHelperPlugin::Create(
      "hello", helper_.WebView()->MainFrame()->ToWebLocalFrame()));
  EXPECT_EQ(0, plugin_.get());
}

}  // namespace

}  // namespace blink
