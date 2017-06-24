// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/PerformanceResourceTiming.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PerformanceResourceTimingTest : public ::testing::Test {
 protected:
  AtomicString GetNextHopProtocol(const AtomicString& alpn_negotiated_protocol,
                                  const AtomicString& connection_info) {
    return PerformanceResourceTiming::GetNextHopProtocol(
        alpn_negotiated_protocol, connection_info);
  }
};

TEST_F(PerformanceResourceTimingTest,
       TestFallbackToConnectionInfoWhenALPNUnknown) {
  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "unknown";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            connection_info);
}

TEST_F(PerformanceResourceTimingTest,
       TestFallbackToHTTPInfoWhenALPNAndConnectionInfoUnknown) {
  AtomicString connection_info = "unknown";
  AtomicString alpn_negotiated_protocol = "unknown";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info), "");
}

TEST_F(PerformanceResourceTimingTest, TestFallbackToHQWhenContainsQuic) {
  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "quic/1spdy/3";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            "hq");
}

TEST_F(PerformanceResourceTimingTest, TestNoChangeWhenOtherwise) {
  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "RandomProtocol";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            alpn_negotiated_protocol);
}
}  // namespace blink
