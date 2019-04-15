// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_web_rtc.h"

using testing::_;

namespace blink {

class SchedulingAffectingFeaturesTest : public SimTest {
 public:
  PageScheduler* PageScheduler() {
    return MainFrame().Scheduler()->GetPageScheduler();
  }
};

TEST_F(SchedulingAffectingFeaturesTest, WebSocketStopsThrottling) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  EXPECT_FALSE(PageScheduler()->OptedOutFromAggressiveThrottlingForTest());
  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre());

  main_resource.Complete(
      "(<script>"
      "  var socket = new WebSocket(\"ws://www.example.com/websocket\");"
      "</script>)");

  EXPECT_TRUE(PageScheduler()->OptedOutFromAggressiveThrottlingForTest());
  EXPECT_THAT(
      PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebSocket));

  MainFrame().ExecuteScript(WebString("socket.close();"));

  EXPECT_FALSE(PageScheduler()->OptedOutFromAggressiveThrottlingForTest());
  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre());
}

TEST_F(SchedulingAffectingFeaturesTest, WebRTCStopsThrottling) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithWebRTC> platform;

  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  EXPECT_FALSE(PageScheduler()->OptedOutFromAggressiveThrottlingForTest());
  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre());

  main_resource.Complete(
      "(<script>"
      "  var data_channel = new RTCPeerConnection();"
      "</script>)");

  EXPECT_TRUE(PageScheduler()->OptedOutFromAggressiveThrottlingForTest());
  EXPECT_THAT(
      PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
      testing::UnorderedElementsAre(SchedulingPolicy::Feature::kWebRTC));

  MainFrame().ExecuteScript(WebString("data_channel.close();"));

  EXPECT_FALSE(PageScheduler()->OptedOutFromAggressiveThrottlingForTest());
  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre());
}

TEST_F(SchedulingAffectingFeaturesTest, CacheControl_NoStore) {
  SimRequest::Params params;
  params.response_http_headers = {{"cache-control", "no-store"}};
  SimRequest main_resource("https://example.com/", "text/html", params);
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png", params);

  LoadURL("https://example.com/");

  main_resource.Complete("<img src=image.png>");

  EXPECT_THAT(
      PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
      testing::UnorderedElementsAre(
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));

  image_resource.Complete();

  EXPECT_THAT(
      PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
      testing::UnorderedElementsAre(
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore,
          SchedulingPolicy::Feature::kSubresourceHasCacheControlNoStore));
}

TEST_F(SchedulingAffectingFeaturesTest, CacheControl_NoCache) {
  SimRequest::Params params;
  params.response_http_headers = {{"cache-control", "no-cache"}};
  SimRequest main_resource("https://example.com/", "text/html", params);
  SimSubresourceRequest image_resource("https://example.com/image.png",
                                       "image/png", params);
  LoadURL("https://example.com/");

  main_resource.Complete("<img src=image.png>");

  EXPECT_THAT(
      PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
      testing::UnorderedElementsAre(
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoCache));

  image_resource.Complete();

  EXPECT_THAT(
      PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
      testing::UnorderedElementsAre(
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoCache,
          SchedulingPolicy::Feature::kSubresourceHasCacheControlNoCache));
}

TEST_F(SchedulingAffectingFeaturesTest, CacheControl_Navigation) {
  SimRequest::Params params;
  params.response_http_headers = {{"cache-control", "no-cache,no-store"}};
  SimRequest main_resource1("https://foo.com/", "text/html", params);
  LoadURL("https://foo.com/");
  main_resource1.Complete();

  EXPECT_THAT(
      PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
      testing::UnorderedElementsAre(
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoCache,
          SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));

  SimRequest main_resource2("https://bar.com/", "text/html");
  LoadURL("https://bar.com/");
  main_resource2.Complete();

  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre());
}

TEST_F(SchedulingAffectingFeaturesTest, EventListener_PageShow) {
  SimRequest main_resource("https://foo.com/", "text/html");
  LoadURL("https://foo.com/");
  main_resource.Complete(
      "(<script>"
      " window.addEventListener(\"pageshow\", () => {}); "
      "</script>)");

  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre(
                  SchedulingPolicy::Feature::kPageShowEventListener));
}

TEST_F(SchedulingAffectingFeaturesTest, EventListener_PageHide) {
  SimRequest main_resource("https://foo.com/", "text/html");
  LoadURL("https://foo.com/");
  main_resource.Complete(
      "(<script>"
      " window.addEventListener(\"pagehide\", () => {}); "
      "</script>)");

  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre(
                  SchedulingPolicy::Feature::kPageHideEventListener));
}

TEST_F(SchedulingAffectingFeaturesTest, EventListener_BeforeUnload) {
  SimRequest main_resource("https://foo.com/", "text/html");
  LoadURL("https://foo.com/");
  main_resource.Complete(
      "(<script>"
      " window.addEventListener(\"beforeunload\", () => {}); "
      "</script>)");

  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre(
                  SchedulingPolicy::Feature::kBeforeUnloadEventListener));
}

TEST_F(SchedulingAffectingFeaturesTest, EventListener_Unload) {
  SimRequest main_resource("https://foo.com/", "text/html");
  LoadURL("https://foo.com/");
  main_resource.Complete(
      "(<script>"
      " window.addEventListener(\"unload\", () => {}); "
      "</script>)");

  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre(
                  SchedulingPolicy::Feature::kUnloadEventListener));
}

TEST_F(SchedulingAffectingFeaturesTest, EventListener_Freeze) {
  SimRequest main_resource("https://foo.com/", "text/html");
  LoadURL("https://foo.com/");
  main_resource.Complete(
      "(<script>"
      " window.addEventListener(\"freeze\", () => {}); "
      "</script>)");

  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre(
                  SchedulingPolicy::Feature::kFreezeEventListener));
}

TEST_F(SchedulingAffectingFeaturesTest, EventListener_Resume) {
  SimRequest main_resource("https://foo.com/", "text/html");
  LoadURL("https://foo.com/");
  main_resource.Complete(
      "(<script>"
      " window.addEventListener(\"resume\", () => {}); "
      "</script>)");

  EXPECT_THAT(PageScheduler()->GetActiveFeaturesOptingOutFromBackForwardCache(),
              testing::UnorderedElementsAre(
                  SchedulingPolicy::Feature::kResumeEventListener));
}

}  // namespace blink
