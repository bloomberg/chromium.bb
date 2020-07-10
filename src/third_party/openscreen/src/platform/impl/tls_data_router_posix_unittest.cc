// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_data_router_posix.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/ip_address.h"
#include "platform/impl/stream_socket_posix.h"
#include "platform/impl/tls_connection_posix.h"
#include "platform/test/fake_clock.h"
#include "platform/test/fake_task_runner.h"

namespace openscreen {
namespace platform {

using testing::_;
using testing::Return;

class TestingDataRouter : public TlsDataRouterPosix {
 public:
  TestingDataRouter(SocketHandleWaiter* waiter) : TlsDataRouterPosix(waiter) {}

  using TlsDataRouterPosix::IsSocketWatched;
  using TlsDataRouterPosix::NetworkingOperation;

  void DeregisterSocketObserver(StreamSocketPosix* socket) override {
    TlsDataRouterPosix::OnSocketDestroyed(socket, true);
  }

  bool AnySocketsWatched() {
    std::unique_lock<std::mutex> lock(socket_mutex_);
    return !watched_stream_sockets_.empty() && !socket_mappings_.empty();
  }

  MOCK_METHOD2(HasTimedOut, bool(Clock::time_point, Clock::duration));

  void SetLastState(NetworkingOperation last_operation) {
    last_operation_ = last_operation;
  }

  void SetLastConnection(TlsConnectionPosix* last_connection) {
    last_connection_processed_ = last_connection;
  }

  // TODO(rwkeane): Remove these methods once RegisterConnection and
  // DeregisterConnection are implimented in TlsDataRouterPosix.
  void AddConnectionForTesting(TlsConnectionPosix* connection) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    connections_.push_back(connection);
  }

  void RemoveConnectionForTesting(TlsConnectionPosix* connection) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    auto it = std::find(connections_.begin(), connections_.end(), connection);
    connections_.erase(it);
  }
};

class MockObserver : public TestingDataRouter::SocketObserver {
  MOCK_METHOD1(OnConnectionPending, void(StreamSocketPosix*));
};

class MockNetworkWaiter final : public SocketHandleWaiter {
 public:
  MOCK_METHOD2(
      AwaitSocketsReadable,
      ErrorOr<std::vector<SocketHandleRef>>(const std::vector<SocketHandleRef>&,
                                            const Clock::duration&));
};

class MockConnection : public TlsConnectionPosix {
 public:
  MockConnection()
      : TlsConnectionPosix(IPAddress::Version::kV4, &task_runner),
        clock(Clock::now()),
        task_runner(&clock) {}
  MOCK_METHOD0(SendAvailableBytes, void());
  MOCK_METHOD0(TryReceiveMessage, void());

 private:
  FakeClock clock;
  FakeTaskRunner task_runner;
};

TEST(TlsNetworkingManagerPosixTest, SocketsWatchedCorrectly) {
  MockNetworkWaiter network_waiter;
  TestingDataRouter network_manager(&network_waiter);
  auto socket = std::make_unique<StreamSocketPosix>(IPAddress::Version::kV4);
  MockObserver observer;

  auto* ptr = socket.get();
  ASSERT_FALSE(network_manager.IsSocketWatched(ptr));

  network_manager.RegisterSocketObserver(std::move(socket), &observer);
  ASSERT_TRUE(network_manager.IsSocketWatched(ptr));
  ASSERT_TRUE(network_manager.AnySocketsWatched());

  network_manager.DeregisterSocketObserver(ptr);
  ASSERT_FALSE(network_manager.IsSocketWatched(ptr));
  ASSERT_FALSE(network_manager.AnySocketsWatched());

  network_manager.DeregisterSocketObserver(ptr);
  ASSERT_FALSE(network_manager.IsSocketWatched(ptr));
  ASSERT_FALSE(network_manager.AnySocketsWatched());
}

TEST(TlsNetworkingManagerPosixTest, ExitsAfterOneCall) {
  MockNetworkWaiter network_waiter;
  TestingDataRouter network_manager(&network_waiter);
  MockConnection connection1;
  MockConnection connection2;
  MockConnection connection3;
  network_manager.AddConnectionForTesting(&connection1);
  network_manager.AddConnectionForTesting(&connection2);
  network_manager.AddConnectionForTesting(&connection3);

  EXPECT_CALL(network_manager, HasTimedOut(_, _)).WillOnce(Return(true));
  EXPECT_CALL(connection1, SendAvailableBytes()).Times(1);
  EXPECT_CALL(connection1, TryReceiveMessage()).Times(0);
  EXPECT_CALL(connection2, SendAvailableBytes()).Times(0);
  EXPECT_CALL(connection2, TryReceiveMessage()).Times(0);
  EXPECT_CALL(connection3, SendAvailableBytes()).Times(0);
  EXPECT_CALL(connection3, TryReceiveMessage()).Times(0);
  network_manager.PerformNetworkingOperations(Clock::duration{0});
}

TEST(TlsNetworkingManagerPosixTest, StartsAfterPrevious) {
  MockNetworkWaiter network_waiter;
  TestingDataRouter network_manager(&network_waiter);
  MockConnection connection1;
  MockConnection connection2;
  MockConnection connection3;
  network_manager.AddConnectionForTesting(&connection1);
  network_manager.AddConnectionForTesting(&connection2);
  network_manager.AddConnectionForTesting(&connection3);
  network_manager.SetLastState(
      TestingDataRouter::NetworkingOperation::kReading);
  network_manager.SetLastConnection(&connection2);

  EXPECT_CALL(network_manager, HasTimedOut(_, _)).WillOnce(Return(true));
  EXPECT_CALL(connection1, TryReceiveMessage()).Times(0);
  EXPECT_CALL(connection1, SendAvailableBytes()).Times(0);
  EXPECT_CALL(connection2, TryReceiveMessage()).Times(0);
  EXPECT_CALL(connection2, SendAvailableBytes()).Times(1);
  EXPECT_CALL(connection3, TryReceiveMessage()).Times(0);
  EXPECT_CALL(connection3, SendAvailableBytes()).Times(0);
  network_manager.PerformNetworkingOperations(Clock::duration{0});
}

TEST(TlsNetworkingManagerPosixTest, HitsAllCallsOnce) {
  MockNetworkWaiter network_waiter;
  TestingDataRouter network_manager(&network_waiter);
  MockConnection connection1;
  MockConnection connection2;
  MockConnection connection3;
  network_manager.AddConnectionForTesting(&connection1);
  network_manager.AddConnectionForTesting(&connection2);
  network_manager.AddConnectionForTesting(&connection3);

  EXPECT_CALL(network_manager, HasTimedOut(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(connection1, TryReceiveMessage()).Times(1);
  EXPECT_CALL(connection1, SendAvailableBytes()).Times(1);
  EXPECT_CALL(connection2, TryReceiveMessage()).Times(1);
  EXPECT_CALL(connection2, SendAvailableBytes()).Times(1);
  EXPECT_CALL(connection3, TryReceiveMessage()).Times(1);
  EXPECT_CALL(connection3, SendAvailableBytes()).Times(1);
  network_manager.PerformNetworkingOperations(Clock::duration{0});
}

TEST(TlsNetworkingManagerPosixTest, HitsAllCallsOnceStartedInMiddle) {
  MockNetworkWaiter network_waiter;
  TestingDataRouter network_manager(&network_waiter);
  MockConnection connection1;
  MockConnection connection2;
  MockConnection connection3;
  network_manager.AddConnectionForTesting(&connection1);
  network_manager.AddConnectionForTesting(&connection2);
  network_manager.AddConnectionForTesting(&connection3);
  network_manager.SetLastState(
      TestingDataRouter::NetworkingOperation::kReading);
  network_manager.SetLastConnection(&connection2);

  EXPECT_CALL(network_manager, HasTimedOut(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(connection1, TryReceiveMessage()).Times(1);
  EXPECT_CALL(connection1, SendAvailableBytes()).Times(1);
  EXPECT_CALL(connection2, TryReceiveMessage()).Times(1);
  EXPECT_CALL(connection2, SendAvailableBytes()).Times(1);
  EXPECT_CALL(connection3, TryReceiveMessage()).Times(1);
  EXPECT_CALL(connection3, SendAvailableBytes()).Times(1);
  network_manager.PerformNetworkingOperations(Clock::duration{0});
}

}  // namespace platform
}  // namespace openscreen
