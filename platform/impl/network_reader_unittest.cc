// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/impl/socket_handle_posix.h"
#include "platform/impl/udp_socket_posix.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/fake_udp_socket.h"

namespace openscreen {
namespace platform {

using namespace ::testing;
using ::testing::_;
using ::testing::Invoke;

class MockUdpSocketPosix : public UdpSocketPosix {
 public:
  explicit MockUdpSocketPosix(TaskRunner* task_runner,
                              Client* client,
                              int fd,
                              Version version = Version::kV4)
      : UdpSocketPosix(task_runner, client, fd, IPEndpoint()),
        version_(version) {}
  ~MockUdpSocketPosix() override = default;

  bool IsIPv4() const override { return version_ == UdpSocket::Version::kV4; }

  bool IsIPv6() const override { return version_ == UdpSocket::Version::kV6; }

  MOCK_METHOD0(Bind, void());
  MOCK_METHOD1(SetMulticastOutboundInterface, void(NetworkInterfaceIndex));
  MOCK_METHOD2(JoinMulticastGroup,
               void(const IPAddress&, NetworkInterfaceIndex));
  MOCK_METHOD0(ReceiveMessage, void());
  MOCK_METHOD3(SendMessage, void(const void*, size_t, const IPEndpoint&));
  MOCK_METHOD1(SetDscp, void(DscpMode));

 private:
  Version version_;
};

// Mock event waiter
class MockNetworkWaiter final : public NetworkWaiter {
 public:
  MOCK_METHOD2(
      AwaitSocketsReadable,
      ErrorOr<std::vector<SocketHandle>>(const std::vector<SocketHandle>&,
                                         const Clock::duration&));
};

// Mock Task Runner
class MockTaskRunner final : public TaskRunner {
 public:
  MockTaskRunner() {
    tasks_posted = 0;
    delayed_tasks_posted = 0;
  }

  void PostPackagedTask(Task t) override {
    tasks_posted++;
    t();
  }

  void PostPackagedTaskWithDelay(Task t, Clock::duration duration) override {
    delayed_tasks_posted++;
    t();
  }

  uint32_t tasks_posted;
  uint32_t delayed_tasks_posted;
};

// Class extending NetworkWaiter to allow for looking at protected data.
class TestingNetworkWaiter final : public NetworkReader {
 public:
  TestingNetworkWaiter(std::unique_ptr<NetworkWaiter> waiter)
      : NetworkReader(std::move(waiter)) {}

  void OnDestroy(UdpSocket* socket) override { OnDelete(socket, true); }

  bool IsMappedRead(UdpSocket* socket) {
    return std::find(sockets_.begin(), sockets_.end(), socket) !=
           sockets_.end();
  }

  // Public method to call wait, since usually this method is internally
  // callable only.
  Error WaitTesting(Clock::duration timeout) { return WaitAndRead(timeout); }
};

TEST(NetworkReaderTest, WatchReadableSucceeds) {
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(new MockNetworkWaiter());
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  FakeUdpSocket::MockClient client;
  std::unique_ptr<MockUdpSocketPosix> socket =
      std::make_unique<MockUdpSocketPosix>(task_runner.get(), &client, 42,
                                           UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));

  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), false);

  network_waiter.OnCreate(socket.get());
  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), true);

  network_waiter.OnCreate(socket.get());
  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), true);
}

TEST(NetworkReaderTest, UnwatchReadableSucceeds) {
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(new MockNetworkWaiter());
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  FakeUdpSocket::MockClient client;
  std::unique_ptr<MockUdpSocketPosix> socket =
      std::make_unique<MockUdpSocketPosix>(task_runner.get(), &client, 17,
                                           UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));

  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));
  network_waiter.OnDestroy(socket.get());
  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));

  network_waiter.OnCreate(socket.get());
  EXPECT_TRUE(network_waiter.IsMappedRead(socket.get()));

  network_waiter.OnDestroy(socket.get());
  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));

  network_waiter.OnDestroy(socket.get());
  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));
}

TEST(NetworkReaderTest, WaitBubblesUpWaitForEventsErrors) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);

  Error::Code response_code = Error::Code::kAgain;
  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::move(response_code))));
  auto result = network_waiter.WaitTesting(timeout);
  EXPECT_EQ(result.code(), response_code);

  response_code = Error::Code::kOperationInvalid;
  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::move(response_code))));
  result = network_waiter.WaitTesting(timeout);
  EXPECT_EQ(result.code(), response_code);
}

TEST(NetworkReaderTest, WaitReturnsSuccessfulOnNoEvents) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);

  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<SocketHandle>{})));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
}

TEST(NetworkReaderTest, WaitSuccessfullyCalledOnAllWatchedSockets) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  FakeUdpSocket::MockClient client;
  std::unique_ptr<MockUdpSocketPosix> socket =
      std::make_unique<MockUdpSocketPosix>(task_runner.get(), &client, 10,
                                           UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);
  UdpPacket packet;

  network_waiter.OnCreate(socket.get());
  EXPECT_CALL(*mock_waiter_ptr,
              AwaitSocketsReadable(ContainerEq<std::vector<SocketHandle>>(
                                       {SocketHandle{socket->GetFd()}}),
                                   timeout))
      .WillOnce(Return(ByMove(std::move(Error::Code::kAgain))));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kAgain);
}

TEST(NetworkReaderTest, WaitSuccessfulReadAndCallCallback) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  auto* task_runner_ptr = new MockTaskRunner();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(task_runner_ptr);
  FakeUdpSocket::MockClient client;
  MockUdpSocketPosix socket(task_runner.get(), &client, 42,
                            UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter));
  auto timeout = Clock::duration(0);
  UdpPacket packet;

  network_waiter.OnCreate(&socket);

  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(
          ByMove(std::vector<SocketHandle>{SocketHandle{socket.GetFd()}})));
  EXPECT_CALL(socket, ReceiveMessage()).Times(1);
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
}

}  // namespace platform
}  // namespace openscreen
