// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/udp_socket.h"

#include "gtest/gtest.h"
#include "platform/test/mock_udp_socket.h"

namespace openscreen {
namespace platform {

// Under some conditions, calling a callback can result in an exception
// "terminate called after throwing an instance of 'std::bad_function_call'"
// which will then crash the running code. This test ensures that deleting a
// new, unmodified UDP Socket object doesn't hit this edge case.
TEST(UdpSocketTest, TestDeletionWithoutCallbackSet) {
  UdpSocket* socket = new MockUdpSocket(UdpSocket::Version::kV4);
  delete socket;
}

TEST(UdpSocketTest, TestCallbackCalledOnDeletion) {
  UdpSocket* socket = new MockUdpSocket(UdpSocket::Version::kV4);
  int call_count = 0;
  std::function<void(UdpSocket*)> callback = [&call_count](UdpSocket* socket) {
    call_count++;
  };
  socket->SetDeletionCallback(callback);

  EXPECT_EQ(call_count, 0);
  delete socket;

  EXPECT_EQ(call_count, 1);
}

}  // namespace platform
}  // namespace openscreen
