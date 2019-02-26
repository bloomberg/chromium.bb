// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_uma_histograms.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace content {

class MockPerSessionWebRTCAPIMetrics : public PerSessionWebRTCAPIMetrics {
 public:
  MockPerSessionWebRTCAPIMetrics() {}

  using PerSessionWebRTCAPIMetrics::LogUsageOnlyOnce;

  MOCK_METHOD1(LogUsage, void(blink::WebRTCAPIName));
};

class PerSessionWebRTCAPIMetricsTest
    : public testing::Test,
      public testing::WithParamInterface<blink::WebRTCAPIName> {
 public:
  PerSessionWebRTCAPIMetricsTest() = default;
  ~PerSessionWebRTCAPIMetricsTest() override = default;

 protected:
  MockPerSessionWebRTCAPIMetrics metrics;
};

TEST_P(PerSessionWebRTCAPIMetricsTest, NoCallOngoing) {
  blink::WebRTCAPIName api_name = GetParam();
  EXPECT_CALL(metrics, LogUsage(api_name)).Times(1);
  metrics.LogUsageOnlyOnce(api_name);
}

TEST_P(PerSessionWebRTCAPIMetricsTest, CallOngoing) {
  blink::WebRTCAPIName api_name = GetParam();
  metrics.IncrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(api_name)).Times(1);
  metrics.LogUsageOnlyOnce(api_name);
}

INSTANTIATE_TEST_CASE_P(
    PerSessionWebRTCAPIMetricsTest,
    PerSessionWebRTCAPIMetricsTest,
    ::testing::ValuesIn({blink::WebRTCAPIName::kGetUserMedia,
                         blink::WebRTCAPIName::kGetDisplayMedia,
                         blink::WebRTCAPIName::kEnumerateDevices,
                         blink::WebRTCAPIName::kRTCPeerConnection}));

TEST(PerSessionWebRTCAPIMetrics, NoCallOngoingMultiplePC) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(blink::WebRTCAPIName::kRTCPeerConnection))
      .Times(1);
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
}

TEST(PerSessionWebRTCAPIMetrics, BeforeAfterCallMultiplePC) {
  MockPerSessionWebRTCAPIMetrics metrics;
  EXPECT_CALL(metrics, LogUsage(blink::WebRTCAPIName::kRTCPeerConnection))
      .Times(1);
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
  metrics.IncrementStreamCounter();
  metrics.IncrementStreamCounter();
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
  metrics.DecrementStreamCounter();
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
  metrics.DecrementStreamCounter();
  EXPECT_CALL(metrics, LogUsage(blink::WebRTCAPIName::kRTCPeerConnection))
      .Times(1);
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
  metrics.LogUsageOnlyOnce(blink::WebRTCAPIName::kRTCPeerConnection);
}

}  // namespace content
