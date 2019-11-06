// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_waiter_posix.h"

#include <sys/socket.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/udp_socket.h"

namespace openscreen {
namespace platform {

using namespace ::testing;
using ::testing::_;

TEST(NetworkWaiterPosixTest, ValidateTimevalConversion) {
  auto timespan = Clock::duration::zero();
  auto timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 0);

  timespan = Clock::duration(1000000);
  timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 1);
  EXPECT_EQ(timeval.tv_usec, 0);

  timespan = Clock::duration(1000000 - 1);
  timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 1000000 - 1);

  timespan = Clock::duration(1);
  timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 1);

  timespan = Clock::duration(100000010);
  timeval = NetworkWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 100);
  EXPECT_EQ(timeval.tv_usec, 10);
}

}  // namespace platform
}  // namespace openscreen
