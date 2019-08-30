// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/udp_socket.h"

#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/fake_udp_socket.h"

namespace openscreen {
namespace platform {
namespace {

class MockCallbacks : public UdpSocket::LifetimeObserver {
 public:
  MOCK_METHOD1(OnCreate, void(UdpSocket*));
  MOCK_METHOD1(OnDestroy, void(UdpSocket*));
};

}  // namespace

using testing::_;

TEST(UdpSocketTest, TestDeletionWithoutCallbackSet) {
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(&clock);
  FakeUdpSocket::MockClient client;
  MockCallbacks callbacks;
  EXPECT_CALL(callbacks, OnCreate(_)).Times(0);
  EXPECT_CALL(callbacks, OnDestroy(_)).Times(0);
  {
    UdpSocket* socket =
        new FakeUdpSocket(&task_runner, &client, UdpSocket::Version::kV4);
    delete socket;
  }
}

TEST(UdpSocketTest, TestCallbackCalledOnDeletion) {
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(&clock);
  FakeUdpSocket::MockClient client;
  MockCallbacks callbacks;
  EXPECT_CALL(callbacks, OnCreate(_)).Times(1);
  EXPECT_CALL(callbacks, OnDestroy(_)).Times(1);
  UdpSocket::SetLifetimeObserver(&callbacks);

  {
    UdpSocket* socket =
        new FakeUdpSocket(&task_runner, &client, UdpSocket::Version::kV4);
    delete socket;
  }

  UdpSocket::SetLifetimeObserver(nullptr);
}

}  // namespace platform
}  // namespace openscreen
