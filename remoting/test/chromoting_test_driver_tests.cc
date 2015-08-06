// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/chromoting_test_driver_environment.h"
#include "remoting/test/chromoting_test_fixture.h"
#include "remoting/test/connection_time_observer.h"

namespace {
const base::TimeDelta kPinBasedMaxConnectionTimeInSeconds =
    base::TimeDelta::FromSeconds(5);
const int kPinBasedMaxAuthenticationTimeMs = 4000;
const int kMaxTimeToConnectMs = 1000;
}

namespace remoting {
namespace test {

TEST_F(ChromotingTestFixture, DISABLED_TestMeasurePinBasedAuthentication) {
  bool connected = ConnectToHost(kPinBasedMaxConnectionTimeInSeconds);
  EXPECT_TRUE(connected);

  Disconnect();
  EXPECT_FALSE(connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::INITIALIZING,
      protocol::ConnectionToHost::State::CLOSED).is_max());

  int authentication_time = connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::INITIALIZING,
      protocol::ConnectionToHost::State::AUTHENTICATED).InMilliseconds();
  EXPECT_LE(authentication_time, kPinBasedMaxAuthenticationTimeMs);

  int authenticated_to_connected_time =
      connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::AUTHENTICATED,
          protocol::ConnectionToHost::State::CONNECTED).InMilliseconds();
  EXPECT_LE(authenticated_to_connected_time, kMaxTimeToConnectMs);
}

TEST_F(ChromotingTestFixture, DISABLED_TestMeasureReconnectPerformance) {
  bool connected = ConnectToHost(kPinBasedMaxConnectionTimeInSeconds);
  EXPECT_TRUE(connected);

  Disconnect();
  EXPECT_FALSE(connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::INITIALIZING,
      protocol::ConnectionToHost::State::CLOSED).is_max());

  int authentication_time = connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::INITIALIZING,
      protocol::ConnectionToHost::State::AUTHENTICATED).InMilliseconds();
  EXPECT_LE(authentication_time, kPinBasedMaxAuthenticationTimeMs);

  int authenticated_to_connected_time =
      connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::AUTHENTICATED,
          protocol::ConnectionToHost::State::CONNECTED).InMilliseconds();
  EXPECT_LE(authenticated_to_connected_time, kMaxTimeToConnectMs);

  // Begin reconnection to same host.
  connected = ConnectToHost(kPinBasedMaxConnectionTimeInSeconds);
  EXPECT_TRUE(connected);

  Disconnect();
  EXPECT_FALSE(connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::INITIALIZING,
      protocol::ConnectionToHost::State::CLOSED).is_max());

  authentication_time = connection_time_observer_->GetStateTransitionTime(
      protocol::ConnectionToHost::State::INITIALIZING,
      protocol::ConnectionToHost::State::AUTHENTICATED).InMilliseconds();
  EXPECT_LE(authentication_time, kPinBasedMaxAuthenticationTimeMs);

  authenticated_to_connected_time =
      connection_time_observer_->GetStateTransitionTime(
          protocol::ConnectionToHost::State::AUTHENTICATED,
          protocol::ConnectionToHost::State::CONNECTED).InMilliseconds();
  EXPECT_LE(authenticated_to_connected_time, kMaxTimeToConnectMs);
}

// TODO(TonyChun): Remove #include "chromoting_test_driver_environment.h" and
// this test once connecting to the lab's Linux host is working.
TEST(HostListTest, VerifyRequestedHostIsInHostList) {
  EXPECT_TRUE(g_chromoting_shared_data);
  EXPECT_TRUE(g_chromoting_shared_data->host_info().IsReadyForConnection());
}

}  // namespace test
}  // namespace remoting
