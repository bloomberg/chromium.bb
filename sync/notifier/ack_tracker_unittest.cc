// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/ack_tracker.h"

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/time/tick_clock.h"
#include "google/cacheinvalidation/include/types.h"
#include "google/cacheinvalidation/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

class FakeTickClock : public base::TickClock {
 public:
  FakeTickClock() {}

  virtual ~FakeTickClock() {}

  void LeapForward(int seconds) {
    ASSERT_GT(seconds, 0);
    fake_now_ticks_ += base::TimeDelta::FromSeconds(seconds);
  }

  // After the next call to Now(), immediately leap forward by |seconds|.
  void DelayedLeapForward(int seconds) {
    ASSERT_GT(seconds, 0);
    delayed_leap_ = base::TimeDelta::FromSeconds(seconds);
  }

  virtual base::TimeTicks NowTicks() OVERRIDE {
    base::TimeTicks fake_now_ticks = fake_now_ticks_;
    if (delayed_leap_ > base::TimeDelta()) {
      fake_now_ticks_ += delayed_leap_;
      delayed_leap_ = base::TimeDelta();
    }
    return fake_now_ticks;
  }

 private:
  base::TimeTicks fake_now_ticks_;
  base::TimeDelta delayed_leap_;
};

class FakeBackoffEntry : public net::BackoffEntry {
 public:
  FakeBackoffEntry(const Policy* const policy, base::TickClock* tick_clock)
      : BackoffEntry(policy),
        tick_clock_(tick_clock) {
  }

 protected:
  virtual base::TimeTicks ImplGetTimeNow() const OVERRIDE {
    return tick_clock_->NowTicks();
  }

 private:
  base::TickClock* const tick_clock_;
};

class MockDelegate : public AckTracker::Delegate {
 public:
  MOCK_METHOD1(OnTimeout, void(const ObjectIdSet&));
};

scoped_ptr<net::BackoffEntry> CreateMockEntry(
    base::TickClock* tick_clock,
    const net::BackoffEntry::Policy* const policy) {
  return scoped_ptr<net::BackoffEntry>(new FakeBackoffEntry(
      policy, tick_clock));
}

}  // namespace

class AckTrackerTest : public testing::Test {
 public:
  AckTrackerTest()
      : ack_tracker_(&fake_tick_clock_, &delegate_),
        kIdOne(ipc::invalidation::ObjectSource::TEST, "one"),
        kIdTwo(ipc::invalidation::ObjectSource::TEST, "two") {
    ack_tracker_.SetCreateBackoffEntryCallbackForTest(
        base::Bind(&CreateMockEntry, &fake_tick_clock_));
  }

 protected:
  bool TriggerTimeoutNow() {
    return ack_tracker_.TriggerTimeoutAtForTest(fake_tick_clock_.NowTicks());
  }

  base::TimeDelta GetTimerDelay() const {
    const base::Timer& timer = ack_tracker_.GetTimerForTest();
    if (!timer.IsRunning())
      ADD_FAILURE() << "Timer is not running!";
    return timer.GetCurrentDelay();
  }

  FakeTickClock fake_tick_clock_;
  ::testing::StrictMock<MockDelegate> delegate_;
  AckTracker ack_tracker_;

  const invalidation::ObjectId kIdOne;
  const invalidation::ObjectId kIdTwo;

  // AckTracker uses base::Timer internally, which depends on the existence of a
  // MessageLoop.
  MessageLoop message_loop_;
};

// Tests that various combinations of Track()/Ack() behave as
// expected.
TEST_F(AckTrackerTest, TrackAndAck) {
  ObjectIdSet ids_one;
  ids_one.insert(kIdOne);
  ObjectIdSet ids_two;
  ids_two.insert(kIdTwo);
  ObjectIdSet ids_all;
  ids_all.insert(kIdOne);
  ids_all.insert(kIdTwo);

  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids_one);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids_two);
  ack_tracker_.Ack(ids_one);
  ack_tracker_.Ack(ids_two);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());

  ack_tracker_.Track(ids_all);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Ack(ids_one);
  ack_tracker_.Ack(ids_two);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());

  ack_tracker_.Track(ids_one);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids_two);
  ack_tracker_.Ack(ids_all);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());

  ack_tracker_.Track(ids_all);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Ack(ids_all);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
}

TEST_F(AckTrackerTest, DoubleTrack) {
  ObjectIdSet ids;
  ids.insert(kIdOne);

  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids);
  ack_tracker_.Ack(ids);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
}

TEST_F(AckTrackerTest, UntrackedAck) {
  ObjectIdSet ids;
  ids.insert(kIdOne);

  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Ack(ids);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
}

TEST_F(AckTrackerTest, Clear) {
  ObjectIdSet ids;
  ids.insert(kIdOne);
  ids.insert(kIdOne);

  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Clear();
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
}

// Test that timeout behavior for one object ID. The timeout should increase
// exponentially until it hits the cap.
TEST_F(AckTrackerTest, SimpleTimeout) {
  ObjectIdSet ids;
  ids.insert(kIdOne);

  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());

  EXPECT_EQ(base::TimeDelta::FromSeconds(60), GetTimerDelay());
  fake_tick_clock_.LeapForward(60);
  EXPECT_CALL(delegate_, OnTimeout(ids));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_EQ(base::TimeDelta::FromSeconds(120), GetTimerDelay());
  fake_tick_clock_.LeapForward(120);
  EXPECT_CALL(delegate_, OnTimeout(ids));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_EQ(base::TimeDelta::FromSeconds(240), GetTimerDelay());
  fake_tick_clock_.LeapForward(240);
  EXPECT_CALL(delegate_, OnTimeout(ids));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_EQ(base::TimeDelta::FromSeconds(480), GetTimerDelay());
  fake_tick_clock_.LeapForward(480);
  EXPECT_CALL(delegate_, OnTimeout(ids));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_EQ(base::TimeDelta::FromSeconds(600), GetTimerDelay());
  fake_tick_clock_.LeapForward(600);
  EXPECT_CALL(delegate_, OnTimeout(ids));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_EQ(base::TimeDelta::FromSeconds(600), GetTimerDelay());
  fake_tick_clock_.LeapForward(600);
  EXPECT_CALL(delegate_, OnTimeout(ids));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Ack(ids);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());

  // The backoff time should be reset after an Ack/Track cycle.
  ack_tracker_.Track(ids);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());

  EXPECT_EQ(base::TimeDelta::FromSeconds(60), GetTimerDelay());
  fake_tick_clock_.LeapForward(60);
  EXPECT_CALL(delegate_, OnTimeout(ids));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Ack(ids);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
}

// Tests that a sequence of Track() calls that results in interleaving
// timeouts occurs as expected.
TEST_F(AckTrackerTest, InterleavedTimeout) {
  ObjectIdSet ids_one;
  ids_one.insert(kIdOne);
  ObjectIdSet ids_two;
  ids_two.insert(kIdTwo);

  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids_one);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());

  fake_tick_clock_.LeapForward(30);
  ack_tracker_.Track(ids_two);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());

  EXPECT_EQ(base::TimeDelta::FromSeconds(60), GetTimerDelay());
  fake_tick_clock_.LeapForward(30);
  EXPECT_CALL(delegate_, OnTimeout(ids_one));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_EQ(base::TimeDelta::FromSeconds(30), GetTimerDelay());
  fake_tick_clock_.LeapForward(30);
  EXPECT_CALL(delegate_, OnTimeout(ids_two));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_EQ(base::TimeDelta::FromSeconds(90), GetTimerDelay());
  fake_tick_clock_.LeapForward(90);
  EXPECT_CALL(delegate_, OnTimeout(ids_one));
  EXPECT_TRUE(TriggerTimeoutNow());

  EXPECT_EQ(base::TimeDelta::FromSeconds(30), GetTimerDelay());
  fake_tick_clock_.LeapForward(30);
  EXPECT_CALL(delegate_, OnTimeout(ids_two));
  EXPECT_TRUE(TriggerTimeoutNow());

  ack_tracker_.Ack(ids_one);
  ack_tracker_.Ack(ids_two);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
}

// Tests that registering a new object ID properly shortens the timeout when
// needed.
TEST_F(AckTrackerTest, ShortenTimeout) {
  ObjectIdSet ids_one;
  ids_one.insert(kIdOne);
  ObjectIdSet ids_two;
  ids_two.insert(kIdTwo);

  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids_one);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());

  EXPECT_EQ(base::TimeDelta::FromSeconds(60), GetTimerDelay());
  fake_tick_clock_.LeapForward(60);
  EXPECT_CALL(delegate_, OnTimeout(ids_one));
  EXPECT_TRUE(TriggerTimeoutNow());

  // Without this next register, the next timeout should occur in 120 seconds
  // from the last timeout event.
  EXPECT_EQ(base::TimeDelta::FromSeconds(120), GetTimerDelay());
  fake_tick_clock_.LeapForward(30);
  ack_tracker_.Track(ids_two);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());

  // Now that we've registered another entry though, we should receive a timeout
  // in 60 seconds.
  EXPECT_EQ(base::TimeDelta::FromSeconds(60), GetTimerDelay());
  fake_tick_clock_.LeapForward(60);
  EXPECT_CALL(delegate_, OnTimeout(ids_two));
  EXPECT_TRUE(TriggerTimeoutNow());

  // Verify that the original timeout for kIdOne still occurs as expected.
  EXPECT_EQ(base::TimeDelta::FromSeconds(30), GetTimerDelay());
  fake_tick_clock_.LeapForward(30);
  EXPECT_CALL(delegate_, OnTimeout(ids_one));
  EXPECT_TRUE(TriggerTimeoutNow());

  ack_tracker_.Ack(ids_one);
  ack_tracker_.Ack(ids_two);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
}

// Tests that a delay between inserting a new object ID registration and start
// the timer that is greater than the initial timeout period (60 seconds) does
// not break things. This could happen on a heavily loaded system, for instance.
TEST_F(AckTrackerTest, ImmediateTimeout) {
  ObjectIdSet ids;
  ids.insert(kIdOne);

  fake_tick_clock_.DelayedLeapForward(90);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
  ack_tracker_.Track(ids);
  EXPECT_FALSE(ack_tracker_.IsQueueEmptyForTest());

  EXPECT_EQ(base::TimeDelta::FromSeconds(0), GetTimerDelay());
  EXPECT_CALL(delegate_, OnTimeout(ids));
  message_loop_.RunUntilIdle();

  // The next timeout should still be scheduled normally.
  EXPECT_EQ(base::TimeDelta::FromSeconds(120), GetTimerDelay());
  fake_tick_clock_.LeapForward(120);
  EXPECT_CALL(delegate_, OnTimeout(ids));
  EXPECT_TRUE(TriggerTimeoutNow());

  ack_tracker_.Ack(ids);
  EXPECT_TRUE(ack_tracker_.IsQueueEmptyForTest());
}

}  // namespace syncer
