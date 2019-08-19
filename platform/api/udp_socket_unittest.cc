// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/udp_socket.h"

#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {

// Under some conditions, calling a callback can result in an exception
// "terminate called after throwing an instance of 'std::bad_function_call'"
// which will then crash the running code. This test ensures that deleting a
// new, unmodified UDP Socket object doesn't hit this edge case.
TEST(UdpSocketTest, TestDeletionWithoutCallbackSet) {
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(&clock);
  MockUdpSocket::MockClient client;
  UdpSocket* socket =
      new MockUdpSocket(&task_runner, &client, UdpSocket::Version::kV4);
  delete socket;
}

TEST(UdpSocketTest, TestCallbackCalledOnDeletion) {
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(&clock);
  MockUdpSocket::MockClient client;
  UdpSocket* socket =
      new MockUdpSocket(&task_runner, &client, UdpSocket::Version::kV4);
  int call_count = 0;
  std::function<void(UdpSocket*)> callback = [&call_count](UdpSocket* socket) {
    call_count++;
  };
  socket->SetDeletionCallback(callback);

  EXPECT_EQ(call_count, 0);
  delete socket;

  EXPECT_EQ(call_count, 1);
}

// Tests that a UdpSocket that does not specify any address or port will
// successfully Bind(), and that the operating system will return the
// auto-assigned socket name (i.e., the local endpoint's port will not be zero).
TEST(UdpSocketTest, ResolvesLocalEndpoint_IPv4) {
  const uint8_t kIpV4AddrAny[4] = {};
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(&clock);
  MockUdpSocket::MockClient client;
  ErrorOr<UdpSocketUniquePtr> create_result = UdpSocket::Create(
      &task_runner, &client, IPEndpoint{IPAddress(kIpV4AddrAny), 0});
  ASSERT_TRUE(create_result) << create_result.error();
  const auto socket = create_result.MoveValue();
  const Error bind_result = socket->Bind();
  ASSERT_TRUE(bind_result.ok()) << bind_result;
  const IPEndpoint local_endpoint = socket->GetLocalEndpoint();
  EXPECT_NE(local_endpoint.port, 0) << local_endpoint;
}

// Tests that a UdpSocket that does not specify any address or port will
// successfully Bind(), and that the operating system will return the
// auto-assigned socket name (i.e., the local endpoint's port will not be zero).
TEST(UdpSocketTest, ResolvesLocalEndpoint_IPv6) {
  const uint8_t kIpV6AddrAny[16] = {};
  FakeClock clock(Clock::now());
  FakeTaskRunner task_runner(&clock);
  MockUdpSocket::MockClient client;
  ErrorOr<UdpSocketUniquePtr> create_result = UdpSocket::Create(
      &task_runner, &client, IPEndpoint{IPAddress(kIpV6AddrAny), 0});
  ASSERT_TRUE(create_result) << create_result.error();
  const auto socket = create_result.MoveValue();
  const Error bind_result = socket->Bind();
  ASSERT_TRUE(bind_result.ok()) << bind_result;
  const IPEndpoint local_endpoint = socket->GetLocalEndpoint();
  EXPECT_NE(local_endpoint.port, 0) << local_endpoint;
}

}  // namespace platform
}  // namespace openscreen
