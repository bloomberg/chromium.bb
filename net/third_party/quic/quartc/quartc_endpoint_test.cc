// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/quartc/quartc_endpoint.h"

#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/quartc/simulated_packet_transport.h"
#include "net/third_party/quic/test_tools/simulator/simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quic {
namespace {

static QuicByteCount kDefaultMaxPacketSize = 1200;

class FakeEndpointDelegate : public QuartcEndpoint::Delegate {
 public:
  void OnSessionCreated(QuartcSession* session) override {
    last_session_ = session;
  }

  void OnConnectError(QuicErrorCode /*error*/,
                      const QuicString& /*error_details*/) override {}

  QuartcSession* last_session() { return last_session_; }

 private:
  QuartcSession* last_session_ = nullptr;
};

class QuartcEndpointTest : public QuicTest {
 protected:
  QuartcEndpointTest()
      : transport_(&simulator_,
                   "client_transport",
                   "server_transport",
                   10 * kDefaultMaxPacketSize),
        endpoint_(simulator_.GetAlarmFactory(),
                  simulator_.GetClock(),
                  &delegate_) {}

  simulator::Simulator simulator_;
  simulator::SimulatedQuartcPacketTransport transport_;
  FakeEndpointDelegate delegate_;
  QuartcEndpoint endpoint_;
};

// The session remains null immediately after a call to Connect.  The caller
// must wait for an async callback before it gets a nonnull session.
TEST_F(QuartcEndpointTest, CreatesSessionAsynchronously) {
  QuartcSessionConfig config;
  config.perspective = Perspective::IS_CLIENT;
  config.packet_transport = &transport_;
  config.max_packet_size = kDefaultMaxPacketSize;
  endpoint_.Connect(config);

  EXPECT_EQ(delegate_.last_session(), nullptr);

  EXPECT_TRUE(simulator_.RunUntil(
      [this] { return delegate_.last_session() != nullptr; }));
}

}  // namespace
}  // namespace quic
