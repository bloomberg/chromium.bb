// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "core/exported/WebViewBase.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/testing/sim/SimRequest.h"
#include "core/testing/sim/SimTest.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/TestingPlatformSupportWithWebRTC.h"
#include "platform/wtf/PtrUtil.h"
#include "public/web/WebScriptSource.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blink {

class ActiveConnectionThrottlingTest : public SimTest {};

TEST_F(ActiveConnectionThrottlingTest, WebSocketStopsThrottling) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  EXPECT_FALSE(WebView().Scheduler()->HasActiveConnectionForTest());

  main_resource.Complete(
      "(<script>"
      "  var socket = new WebSocket(\"ws://www.example.com/websocket\");"
      "</script>)");

  EXPECT_TRUE(WebView().Scheduler()->HasActiveConnectionForTest());

  MainFrame().ExecuteScript(WebString("socket.close();"));

  EXPECT_FALSE(WebView().Scheduler()->HasActiveConnectionForTest());
}

TEST_F(ActiveConnectionThrottlingTest, WebRTCStopsThrottling) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithWebRTC> platform;

  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  EXPECT_FALSE(WebView().Scheduler()->HasActiveConnectionForTest());

  main_resource.Complete(
      "(<script>"
      "  var data_channel = new RTCPeerConnection();"
      "</script>)");

  EXPECT_TRUE(WebView().Scheduler()->HasActiveConnectionForTest());

  MainFrame().ExecuteScript(WebString("data_channel.close();"));

  EXPECT_FALSE(WebView().Scheduler()->HasActiveConnectionForTest());
}

}  // namespace blink
