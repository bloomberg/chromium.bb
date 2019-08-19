// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"
#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {

using namespace ::testing;
using ::testing::_;
using ::testing::Invoke;

// Mock event waiter
class MockNetworkWaiter final : public NetworkWaiter {
 public:
  MOCK_METHOD2(AwaitSocketsReadable,
               ErrorOr<std::vector<UdpSocket*>>(const std::vector<UdpSocket*>&,
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
  TestingNetworkWaiter(std::unique_ptr<NetworkWaiter> waiter,
                       TaskRunner* task_runner)
      : NetworkReader(task_runner, std::move(waiter)) {}

  bool IsMappedRead(UdpSocket* socket) {
    return read_callbacks_.find(socket) != read_callbacks_.end();
  }

  // Public method to call wait, since usually this method is internally
  // callable only.
  Error WaitTesting(Clock::duration timeout) { return WaitAndRead(timeout); }
};

class MockCallbacks {
 public:
  std::function<void(UdpPacket)> GetReadCallback() {
    return [this](UdpPacket packet) { this->ReadCallback(std::move(packet)); };
  }

  std::function<void()> GetWriteCallback() {
    return [this]() { this->WriteCallback(); };
  }

  void ReadCallback(UdpPacket packet) { ReadCallbackInternal(); }

  MOCK_METHOD0(ReadCallbackInternal, void());
  MOCK_METHOD0(WriteCallback, void());
};

TEST(NetworkReaderTest, WatchReadableSucceeds) {
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(new MockNetworkWaiter());
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  MockUdpSocket::MockClient client;
  std::unique_ptr<MockUdpSocket> socket = std::make_unique<MockUdpSocket>(
      task_runner.get(), &client, UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter),
                                      task_runner.get());
  MockCallbacks callbacks;

  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), false);

  auto callback = callbacks.GetReadCallback();
  EXPECT_EQ(network_waiter.ReadRepeatedly(socket.get(), callback).code(),
            Error::Code::kNone);

  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), true);

  auto callback2 = callbacks.GetReadCallback();
  EXPECT_EQ(network_waiter.ReadRepeatedly(socket.get(), callback2).code(),
            Error::Code::kIOFailure);

  EXPECT_EQ(network_waiter.IsMappedRead(socket.get()), true);

  // Set deletion callback because otherwise the destructor tries to call a
  // callback on the deleted object when it goes out of scope.
  socket->SetDeletionCallback([](UdpSocket* socket) {});
}

TEST(NetworkReaderTest, UnwatchReadableSucceeds) {
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(new MockNetworkWaiter());
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  MockUdpSocket::MockClient client;
  std::unique_ptr<MockUdpSocket> socket = std::make_unique<MockUdpSocket>(
      task_runner.get(), &client, UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter),
                                      task_runner.get());
  MockCallbacks callbacks;

  auto callback = callbacks.GetReadCallback();
  EXPECT_EQ(network_waiter.CancelRead(socket.get()),
            Error::Code::kOperationInvalid);
  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));

  EXPECT_EQ(network_waiter.ReadRepeatedly(socket.get(), callback).code(),
            Error::Code::kNone);

  EXPECT_EQ(network_waiter.CancelRead(socket.get()), Error::Code::kNone);
  EXPECT_FALSE(network_waiter.IsMappedRead(socket.get()));

  EXPECT_EQ(network_waiter.CancelRead(socket.get()),
            Error::Code::kOperationInvalid);

  // Set deletion callback because otherwise the destructor tries to call a
  // callback on the deleted object when it goes out of scope.
  socket->SetDeletionCallback([](UdpSocket* socket) {});
}

TEST(NetworkReaderTest, WaitBubblesUpWaitForEventsErrors) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  TestingNetworkWaiter network_waiter(std::move(mock_waiter),
                                      task_runner.get());
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
  TestingNetworkWaiter network_waiter(std::move(mock_waiter),
                                      task_runner.get());
  auto timeout = Clock::duration(0);

  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{})));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
}

TEST(NetworkReaderTest, WaitSuccessfullyCalledOnAllWatchedSockets) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  MockUdpSocket::MockClient client;
  std::unique_ptr<MockUdpSocket> socket = std::make_unique<MockUdpSocket>(
      task_runner.get(), &client, UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter),
                                      task_runner.get());
  auto timeout = Clock::duration(0);
  UdpPacket packet;
  MockCallbacks callbacks;

  network_waiter.ReadRepeatedly(socket.get(), callbacks.GetReadCallback());
  EXPECT_CALL(
      *mock_waiter_ptr,
      AwaitSocketsReadable(ContainerEq<std::vector<UdpSocket*>>({socket.get()}),
                           timeout))
      .WillOnce(Return(ByMove(std::move(Error::Code::kAgain))));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kAgain);

  // Set deletion callback because otherwise the destructor tries to call a
  // callback on the deleted object when it goes out of scope.
  socket->SetDeletionCallback([](UdpSocket* socket) {});
}

TEST(NetworkReaderTest, WaitSuccessfulReadAndCallCallback) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  auto* task_runner_ptr = new MockTaskRunner();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(task_runner_ptr);
  MockUdpSocket::MockClient client;
  MockUdpSocket socket(task_runner.get(), &client, UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter),
                                      task_runner.get());
  auto timeout = Clock::duration(0);
  UdpPacket packet;
  MockCallbacks callbacks;

  network_waiter.ReadRepeatedly(&socket, callbacks.GetReadCallback());

  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{&socket})));
  EXPECT_CALL(callbacks, ReadCallbackInternal()).Times(1);
  EXPECT_CALL(socket, ReceiveMessage())
      .WillOnce(Return(ByMove(std::move(packet))));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kNone);
  EXPECT_EQ(task_runner_ptr->tasks_posted, uint32_t{1});

  // Set deletion callback because otherwise the destructor tries to call a
  // callback on the deleted object when it goes out of scope.
  socket.SetDeletionCallback([](UdpSocket* socket) {});
}

TEST(NetworkReaderTest, WaitFailsIfReadingSocketFails) {
  auto* mock_waiter_ptr = new MockNetworkWaiter();
  std::unique_ptr<NetworkWaiter> mock_waiter =
      std::unique_ptr<NetworkWaiter>(mock_waiter_ptr);
  std::unique_ptr<TaskRunner> task_runner =
      std::unique_ptr<TaskRunner>(new MockTaskRunner());
  MockUdpSocket::MockClient client;
  MockUdpSocket socket(task_runner.get(), &client, UdpSocket::Version::kV4);
  TestingNetworkWaiter network_waiter(std::move(mock_waiter),
                                      task_runner.get());
  auto timeout = Clock::duration(0);
  MockCallbacks callbacks;

  network_waiter.ReadRepeatedly(&socket, callbacks.GetReadCallback());

  EXPECT_CALL(*mock_waiter_ptr, AwaitSocketsReadable(_, timeout))
      .WillOnce(Return(ByMove(std::vector<UdpSocket*>{&socket})));
  EXPECT_CALL(callbacks, ReadCallbackInternal()).Times(0);
  EXPECT_CALL(socket, ReceiveMessage())
      .WillOnce(Return(ByMove(Error::Code::kUnknownError)));
  EXPECT_EQ(network_waiter.WaitTesting(timeout), Error::Code::kUnknownError);

  // Set deletion callback because otherwise the destructor tries to call a
  // callback on the deleted object when it goes out of scope.
  socket.SetDeletionCallback([](UdpSocket* socket) {});
}

}  // namespace platform
}  // namespace openscreen
