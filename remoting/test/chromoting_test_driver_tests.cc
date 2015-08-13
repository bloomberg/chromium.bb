// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/chromoting_test_fixture.h"
#include "remoting/test/connection_time_observer.h"

namespace {
const base::TimeDelta kPinBasedMaxConnectionTimeInSeconds =
    base::TimeDelta::FromSeconds(4);
const int kPinBasedMaxAuthenticationTimeMs = 2000;
const int kMaxTimeToConnectMs = 2000;
}

namespace remoting {
namespace test {

TEST_F(ChromotingTestFixture, TestMeasurePinBasedAuthentication) {
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

  connection_time_observer_->DisplayConnectionStats();
}

TEST_F(ChromotingTestFixture, TestMeasureReconnectPerformance) {
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

  connection_time_observer_->DisplayConnectionStats();

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

  connection_time_observer_->DisplayConnectionStats();
}

}  // namespace test
}  // namespace remoting
