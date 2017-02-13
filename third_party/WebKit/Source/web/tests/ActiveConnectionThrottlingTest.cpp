// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "platform/testing/TestingPlatformSupport.h"
#include "public/platform/WebRTCError.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "public/web/WebScriptSource.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebViewScheduler.h"
#include "web/WebLocalFrameImpl.h"
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

  mainFrame().executeScript(WebString("socket.close();"));

  EXPECT_FALSE(webView().scheduler()->hasActiveConnectionForTest());
}

namespace {

class MockWebRTCPeerConnectionHandler : public WebRTCPeerConnectionHandler {
 public:
  MockWebRTCPeerConnectionHandler() {}
  ~MockWebRTCPeerConnectionHandler() override {}

  bool initialize(const WebRTCConfiguration&,
                  const WebMediaConstraints&) override {
    return true;
  }

  void createOffer(const WebRTCSessionDescriptionRequest&,
                   const WebMediaConstraints&) override {}
  void createOffer(const WebRTCSessionDescriptionRequest&,
                   const WebRTCOfferOptions&) override {}
  void createAnswer(const WebRTCSessionDescriptionRequest&,
                    const WebMediaConstraints&) override {}
  void createAnswer(const WebRTCSessionDescriptionRequest&,
                    const WebRTCAnswerOptions&) override {}
  void setLocalDescription(const WebRTCVoidRequest&,
                           const WebRTCSessionDescription&) override {}
  void setRemoteDescription(const WebRTCVoidRequest&,
                            const WebRTCSessionDescription&) override {}
  WebRTCSessionDescription localDescription() override {
    return WebRTCSessionDescription();
  }
  WebRTCSessionDescription remoteDescription() override {
    return WebRTCSessionDescription();
  }
  WebRTCErrorType setConfiguration(const WebRTCConfiguration&) override {
    return WebRTCErrorType::kNone;
  }
  bool addStream(const WebMediaStream&, const WebMediaConstraints&) override {
    return true;
  }
  void removeStream(const WebMediaStream&) override {}
  void getStats(const WebRTCStatsRequest&) override {}
  void getStats(std::unique_ptr<WebRTCStatsReportCallback>) override {}
  WebRTCDataChannelHandler* createDataChannel(
      const WebString& label,
      const WebRTCDataChannelInit&) override {
    return nullptr;
  }
  WebRTCDTMFSenderHandler* createDTMFSender(
      const WebMediaStreamTrack&) override {
    return nullptr;
  }
  void stop() override {}
};

class TestingPlatformSupportWithWebRTC : public TestingPlatformSupport {
 public:
  blink::WebRTCPeerConnectionHandler* createRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient*) override {
    return new MockWebRTCPeerConnectionHandler();
  }
};

}  // namespace

TEST_F(ActiveConnectionThrottlingTest, WebRTCStopsThrottling) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithWebRTC> platform;

  SimRequest mainResource("https://example.com/", "text/html");

  loadURL("https://example.com/");

  EXPECT_FALSE(webView().scheduler()->hasActiveConnectionForTest());

  mainResource.complete(
      "(<script>"
      "  var data_channel = new RTCPeerConnection();"
      "</script>)");

  EXPECT_TRUE(webView().scheduler()->hasActiveConnectionForTest());

  mainFrame().executeScript(WebString("data_channel.close();"));

  EXPECT_FALSE(webView().scheduler()->hasActiveConnectionForTest());
}

}  // namespace blink
