// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/socket_handle_waiter_posix.h"

#include <sys/socket.h>

#include <chrono>
#include <iostream>
#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/impl/socket_handle_posix.h"

namespace openscreen {
namespace platform {
namespace {

using namespace ::testing;
using ::testing::_;

class MockSubscriber : public SocketHandleWaiter::Subscriber {
 public:
  using SocketHandleRef = SocketHandleWaiter::SocketHandleRef;
  MOCK_METHOD1(ProcessReadyHandle, void(SocketHandleRef));
};

class TestingSocketHandleWaiter : public SocketHandleWaiter {
 public:
  using SocketHandleRef = SocketHandleWaiter::SocketHandleRef;

  MOCK_METHOD2(
      AwaitSocketsReadable,
      ErrorOr<std::vector<SocketHandleRef>>(const std::vector<SocketHandleRef>&,
                                            const Clock::duration&));
};

}  // namespace

TEST(SocketHandleWaiterTest, BubblesUpAwaitSocketsReadableErrors) {
  MockSubscriber subscriber;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0{0};
  SocketHandle handle1{1};
  SocketHandle handle2{2};
  const SocketHandle& handle0_ref = handle0;
  const SocketHandle& handle1_ref = handle1;
  const SocketHandle& handle2_ref = handle2;

  waiter.Subscribe(&subscriber, std::cref(handle0_ref));
  waiter.Subscribe(&subscriber, std::cref(handle1_ref));
  waiter.Subscribe(&subscriber, std::cref(handle2_ref));
  Error::Code response = Error::Code::kAgain;
  EXPECT_CALL(subscriber, ProcessReadyHandle(_)).Times(0);
  EXPECT_CALL(waiter, AwaitSocketsReadable(_, _))
      .WillOnce(Return(ByMove(response)));
  waiter.ProcessHandles(Clock::duration{0});
}

TEST(SocketHandleWaiterTest, WatchedSocketsReturnedToCorrectSubscribers) {
  MockSubscriber subscriber;
  MockSubscriber subscriber2;
  TestingSocketHandleWaiter waiter;
  SocketHandle handle0{0};
  SocketHandle handle1{1};
  SocketHandle handle2{2};
  SocketHandle handle3{3};
  const SocketHandle& handle0_ref = handle0;
  const SocketHandle& handle1_ref = handle1;
  const SocketHandle& handle2_ref = handle2;
  const SocketHandle& handle3_ref = handle3;

  waiter.Subscribe(&subscriber, std::cref(handle0_ref));
  waiter.Subscribe(&subscriber, std::cref(handle2_ref));
  waiter.Subscribe(&subscriber2, std::cref(handle1_ref));
  waiter.Subscribe(&subscriber2, std::cref(handle3_ref));
  EXPECT_CALL(subscriber, ProcessReadyHandle(std::cref(handle0_ref))).Times(1);
  EXPECT_CALL(subscriber, ProcessReadyHandle(std::cref(handle2_ref))).Times(1);
  EXPECT_CALL(subscriber2, ProcessReadyHandle(std::cref(handle1_ref))).Times(1);
  EXPECT_CALL(subscriber2, ProcessReadyHandle(std::cref(handle3_ref))).Times(1);
  EXPECT_CALL(waiter, AwaitSocketsReadable(_, _))
      .WillOnce(Return(ByMove(std::vector<SocketHandleWaiter::SocketHandleRef>{
          std::cref(handle0_ref), std::cref(handle1_ref),
          std::cref(handle2_ref), std::cref(handle3_ref)})));
  waiter.ProcessHandles(Clock::duration{0});
}

TEST(SocketHandleWaiterTest, ValidateTimevalConversion) {
  auto timespan = Clock::duration::zero();
  auto timeval = SocketHandleWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 0);

  timespan = Clock::duration(1000000);
  timeval = SocketHandleWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 1);
  EXPECT_EQ(timeval.tv_usec, 0);

  timespan = Clock::duration(1000000 - 1);
  timeval = SocketHandleWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 1000000 - 1);

  timespan = Clock::duration(1);
  timeval = SocketHandleWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 0);
  EXPECT_EQ(timeval.tv_usec, 1);

  timespan = Clock::duration(100000010);
  timeval = SocketHandleWaiterPosix::ToTimeval(timespan);
  EXPECT_EQ(timeval.tv_sec, 100);
  EXPECT_EQ(timeval.tv_usec, 10);
}

}  // namespace platform
}  // namespace openscreen
