// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_epoll_connection_helper.h"

#include "net/quic/crypto/quic_random.h"
#include "net/tools/quic/test_tools/mock_epoll_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::tools::test::MockEpollServer;

namespace net {
namespace tools {
namespace test {
namespace {

class TestDelegate : public QuicAlarm::Delegate {
 public:
  TestDelegate() : fired_(false) {}

  QuicTime OnAlarm() override {
    fired_ = true;
    return QuicTime::Zero();
  }

  bool fired() const { return fired_; }

 private:
  bool fired_;
};

class QuicEpollConnectionHelperTest : public ::testing::Test {
 protected:
  QuicEpollConnectionHelperTest() : helper_(&epoll_server_) {}

  MockEpollServer epoll_server_;
  QuicEpollConnectionHelper helper_;
};

TEST_F(QuicEpollConnectionHelperTest, GetClock) {
  const QuicClock* clock = helper_.GetClock();
  QuicTime start = clock->Now();

  QuicTime::Delta delta = QuicTime::Delta::FromMilliseconds(5);
  epoll_server_.AdvanceBy(delta.ToMicroseconds());

  EXPECT_EQ(start.Add(delta), clock->Now());
}

TEST_F(QuicEpollConnectionHelperTest, GetRandomGenerator) {
  QuicRandom* random = helper_.GetRandomGenerator();
  EXPECT_EQ(QuicRandom::GetInstance(), random);
}

TEST_F(QuicEpollConnectionHelperTest, CreateAlarm) {
  TestDelegate* delegate = new TestDelegate();
  scoped_ptr<QuicAlarm> alarm(helper_.CreateAlarm(delegate));

  const QuicClock* clock = helper_.GetClock();
  QuicTime start = clock->Now();
  QuicTime::Delta delta = QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(start.Add(delta));

  epoll_server_.AdvanceByAndWaitForEventsAndExecuteCallbacks(
      delta.ToMicroseconds());
  EXPECT_EQ(start.Add(delta), clock->Now());
}

TEST_F(QuicEpollConnectionHelperTest, CreateAlarmAndCancel) {
  TestDelegate* delegate = new TestDelegate();
  scoped_ptr<QuicAlarm> alarm(helper_.CreateAlarm(delegate));

  const QuicClock* clock = helper_.GetClock();
  QuicTime start = clock->Now();
  QuicTime::Delta delta = QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(start.Add(delta));
  alarm->Cancel();

  epoll_server_.AdvanceByExactlyAndCallCallbacks(delta.ToMicroseconds());
  EXPECT_EQ(start.Add(delta), clock->Now());
  EXPECT_FALSE(delegate->fired());
}

TEST_F(QuicEpollConnectionHelperTest, CreateAlarmAndReset) {
  TestDelegate* delegate = new TestDelegate();
  scoped_ptr<QuicAlarm> alarm(helper_.CreateAlarm(delegate));

  const QuicClock* clock = helper_.GetClock();
  QuicTime start = clock->Now();
  QuicTime::Delta delta = QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock->Now().Add(delta));
  alarm->Cancel();
  QuicTime::Delta new_delta = QuicTime::Delta::FromMicroseconds(3);
  alarm->Set(clock->Now().Add(new_delta));

  epoll_server_.AdvanceByExactlyAndCallCallbacks(delta.ToMicroseconds());
  EXPECT_EQ(start.Add(delta), clock->Now());
  EXPECT_FALSE(delegate->fired());

  epoll_server_.AdvanceByExactlyAndCallCallbacks(
      new_delta.Subtract(delta).ToMicroseconds());
  EXPECT_EQ(start.Add(new_delta), clock->Now());
  EXPECT_TRUE(delegate->fired());
}

TEST_F(QuicEpollConnectionHelperTest, CreateAlarmAndUpdate) {
  TestDelegate* delegate = new TestDelegate();
  scoped_ptr<QuicAlarm> alarm(helper_.CreateAlarm(delegate));

  const QuicClock* clock = helper_.GetClock();
  QuicTime start = clock->Now();
  QuicTime::Delta delta = QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock->Now().Add(delta));
  QuicTime::Delta new_delta = QuicTime::Delta::FromMicroseconds(3);
  alarm->Update(clock->Now().Add(new_delta),
                QuicTime::Delta::FromMicroseconds(1));

  epoll_server_.AdvanceByExactlyAndCallCallbacks(delta.ToMicroseconds());
  EXPECT_EQ(start.Add(delta), clock->Now());
  EXPECT_FALSE(delegate->fired());

  // Move the alarm forward 1us and ensure it doesn't move forward.
  alarm->Update(clock->Now().Add(new_delta),
                QuicTime::Delta::FromMicroseconds(2));

  epoll_server_.AdvanceByExactlyAndCallCallbacks(
      new_delta.Subtract(delta).ToMicroseconds());
  EXPECT_EQ(start.Add(new_delta), clock->Now());
  EXPECT_TRUE(delegate->fired());

  // Set the alarm via an update call.
  new_delta = QuicTime::Delta::FromMicroseconds(5);
  alarm->Update(clock->Now().Add(new_delta),
                QuicTime::Delta::FromMicroseconds(1));
  EXPECT_TRUE(alarm->IsSet());

  // Update it with an uninitialized time and ensure it's cancelled.
  alarm->Update(QuicTime::Zero(), QuicTime::Delta::FromMicroseconds(1));
  EXPECT_FALSE(alarm->IsSet());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
