// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebViewScheduler.h"
#include "web/WebViewImpl.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

using testing::_;

namespace blink {

class ActiveConnectionThrottlingTest : public SimTest {};

TEST_F(ActiveConnectionThrottlingTest, WebSocketStopsThrottling) {
  SimRequest mainResource("https://example.com/", "text/html");

  loadURL("https://example.com/");

  EXPECT_FALSE(webView().scheduler()->hasActiveConnectionForTest());

  mainResource.complete(
      "(<script>"
      "  var socket = new WebSocket(\"ws://www.example.com/websocket\");"
      "</script>)");

  EXPECT_TRUE(webView().scheduler()->hasActiveConnectionForTest());
}

}  // namespace blink
