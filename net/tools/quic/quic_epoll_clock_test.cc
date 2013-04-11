// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_epoll_clock.h"

#include "net/tools/quic/test_tools/mock_epoll_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
namespace tools {
namespace test {

TEST(QuicEpollClockTest, ApproximateNowInUsec) {
  MockEpollServer epoll_server;
  QuicEpollClock clock(&epoll_server);

  epoll_server.set_now_in_usec(1000000);
  EXPECT_EQ(1000000,
            clock.ApproximateNow().Subtract(QuicTime::Zero()).ToMicroseconds());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1000000),
            clock.NowAsDeltaSinceUnixEpoch());

  epoll_server.AdvanceBy(5);
  EXPECT_EQ(1000005,
            clock.ApproximateNow().Subtract(QuicTime::Zero()).ToMicroseconds());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1000005),
            clock.NowAsDeltaSinceUnixEpoch());
}

TEST(QuicEpollClockTest, NowInUsec) {
  MockEpollServer epoll_server;
  QuicEpollClock clock(&epoll_server);

  epoll_server.set_now_in_usec(1000000);
  EXPECT_EQ(1000000,
            clock.Now().Subtract(QuicTime::Zero()).ToMicroseconds());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1000000),
            clock.NowAsDeltaSinceUnixEpoch());

  epoll_server.AdvanceBy(5);
  EXPECT_EQ(1000005,
            clock.Now().Subtract(QuicTime::Zero()).ToMicroseconds());
  EXPECT_EQ(QuicTime::Delta::FromMicroseconds(1000005),
            clock.NowAsDeltaSinceUnixEpoch());
}

}  // namespace test
}  // namespace tools
}  // namespace net
