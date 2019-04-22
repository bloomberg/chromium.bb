// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_TEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_TEST_H_

#include <gtest/gtest.h>
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_network.h"
#include "third_party/blink/renderer/core/testing/sim/sim_page.h"

namespace blink {

class WebViewImpl;
class WebLocalFrameImpl;
class Document;
class LocalDOMWindow;

class SimTest : public testing::Test {
 protected:
  SimTest();
  ~SimTest() override;

  void SetUp() override;

  void LoadURL(const String& url);

  // WebView is created after SetUp to allow test to customize
  // web runtime features.
  // These methods should be accessed inside test body after a call to SetUp.
  LocalDOMWindow& Window();
  SimPage& GetPage();
  Document& GetDocument();
  WebViewImpl& WebView();
  WebLocalFrameImpl& MainFrame();
  frame_test_helpers::TestWebViewClient& WebViewClient();
  frame_test_helpers::TestWebWidgetClient& WebWidgetClient();
  frame_test_helpers::TestWebFrameClient& WebFrameClient();
  SimCompositor& Compositor();

  Vector<String>& ConsoleMessages();

 private:
  SimNetwork network_;
  SimCompositor compositor_;
  frame_test_helpers::TestWebFrameClient web_frame_client_;
  frame_test_helpers::TestWebWidgetClient web_widget_client_;
  frame_test_helpers::TestWebViewClient web_view_client_;
  SimPage page_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

}  // namespace blink

#endif
