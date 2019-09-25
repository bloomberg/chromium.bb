// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/tls_data_router_posix.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/base/ip_address.h"
#include "platform/impl/stream_socket_posix.h"

namespace openscreen {
namespace platform {

class TestingDataRouter : public TlsDataRouterPosix {
 public:
  TestingDataRouter(NetworkWaiter* waiter) : TlsDataRouterPosix(waiter) {}

  using TlsDataRouterPosix::IsSocketWatched;

  void OnSocketDestroyed(StreamSocketPosix* socket) override {
    TlsDataRouterPosix::OnSocketDestroyed(socket, true);
  }
};

class MockObserver : public TestingDataRouter::SocketObserver {
  MOCK_METHOD1(OnConnectionPending, void(StreamSocketPosix*));
};

class MockNetworkWaiter final : public NetworkWaiter {
 public:
  MOCK_METHOD2(
      AwaitSocketsReadable,
      ErrorOr<std::vector<SocketHandleRef>>(const std::vector<SocketHandleRef>&,
                                            const Clock::duration&));
};

TEST(TlsNetworkingManagerPosixTest, SocketsWatchedCorrectly) {
  MockNetworkWaiter network_waiter;
  TestingDataRouter network_manager(&network_waiter);
  StreamSocketPosix socket(IPAddress::Version::kV4);
  MockObserver observer;

  ASSERT_FALSE(network_manager.IsSocketWatched(&socket));

  network_manager.RegisterSocketObserver(&socket, &observer);
  ASSERT_TRUE(network_manager.IsSocketWatched(&socket));

  network_manager.DeregisterSocketObserver(&socket);
  ASSERT_FALSE(network_manager.IsSocketWatched(&socket));

  network_manager.RegisterSocketObserver(&socket, &observer);
  ASSERT_TRUE(network_manager.IsSocketWatched(&socket));

  network_manager.OnSocketDestroyed(&socket);
  ASSERT_FALSE(network_manager.IsSocketWatched(&socket));
}

}  // namespace platform
}  // namespace openscreen
