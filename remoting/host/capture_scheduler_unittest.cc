// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/capture_scheduler.h"

#include "base/message_loop/message_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/timer/mock_timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

static const int kTestInputs[] = { 100, 50, 30, 20, 10, 30, 60, 80 };
static const int kMinumumFrameIntervalMs = 50;

class CaptureSchedulerTest : public testing::Test {
 public:
  CaptureSchedulerTest() : capture_called_(false) {}

  void InitScheduler() {
    scheduler_.reset(new CaptureScheduler(
        base::Bind(&CaptureSchedulerTest::DoCapture, base::Unretained(this))));
    scheduler_->set_minimum_interval(
        base::TimeDelta::FromMilliseconds(kMinumumFrameIntervalMs));
    tick_clock_ = new base::SimpleTestTickClock();
    scheduler_->SetTickClockForTest(make_scoped_ptr(tick_clock_));
    capture_timer_ = new base::MockTimer(false, false);
    scheduler_->SetTimerForTest(make_scoped_ptr(capture_timer_));
    scheduler_->Start();
  }

  void DoCapture() {
    capture_called_ = true;
  }

  void CheckCaptureCalled() {
    EXPECT_TRUE(capture_called_);
    capture_called_ = false;
  }

  void SimulateSingleFrameCapture(
      base::TimeDelta capture_delay,
      base::TimeDelta encode_delay,
      base::TimeDelta expected_delay_between_frames) {
    capture_timer_->Fire();
    CheckCaptureCalled();
    tick_clock_->Advance(capture_delay);
    scheduler_->OnCaptureCompleted();
    scheduler_->OnFrameEncoded(encode_delay);
    scheduler_->OnFrameSent();

    EXPECT_TRUE(capture_timer_->IsRunning());
    EXPECT_EQ(std::max(base::TimeDelta(),
                       expected_delay_between_frames - capture_delay),
              capture_timer_->GetCurrentDelay());
  }

 protected:
  base::MessageLoop message_loop_;

  scoped_ptr<CaptureScheduler> scheduler_;

  // Owned by |scheduler_|.
  base::SimpleTestTickClock* tick_clock_;
  base::MockTimer* capture_timer_;

  bool capture_called_;
};

TEST_F(CaptureSchedulerTest, SingleSampleSameTimes) {
  const int kTestResults[][arraysize(kTestInputs)] = {
    { 400, 200, 120, 80, 50, 120, 240, 320 }, // One core.
    { 200, 100, 60, 50, 50, 60, 120, 160 },   // Two cores.
    { 100, 50, 50, 50, 50, 50, 60, 80 },      // Four cores.
    { 50, 50, 50, 50, 50, 50, 50, 50 }        // Eight cores.
  };

  for (size_t i = 0; i < arraysize(kTestResults); ++i) {
    for (size_t j = 0; j < arraysize(kTestInputs); ++j) {
      InitScheduler();
      scheduler_->SetNumOfProcessorsForTest(1 << i);

      SimulateSingleFrameCapture(
          base::TimeDelta::FromMilliseconds(kTestInputs[j]),
          base::TimeDelta::FromMilliseconds(kTestInputs[j]),
          base::TimeDelta::FromMilliseconds(kTestResults[i][j]));
    }
  }
}

TEST_F(CaptureSchedulerTest, SingleSampleDifferentTimes) {
  const int kTestResults[][arraysize(kTestInputs)] = {
    { 360, 220, 120, 60, 60, 120, 220, 360 }, // One core.
    { 180, 110, 60, 50, 50, 60, 110, 180 },   // Two cores.
    { 90, 55, 50, 50, 50, 50, 55, 90 },       // Four cores.
    { 50, 50, 50, 50, 50, 50, 50, 50 }        // Eight cores.
  };

  for (size_t i = 0; i < arraysize(kTestResults); ++i) {
    for (size_t j = 0; j < arraysize(kTestInputs); ++j) {
      InitScheduler();
      scheduler_->SetNumOfProcessorsForTest(1 << i);

      SimulateSingleFrameCapture(
          base::TimeDelta::FromMilliseconds(kTestInputs[j]),
          base::TimeDelta::FromMilliseconds(
              kTestInputs[arraysize(kTestInputs) - 1 - j]),
          base::TimeDelta::FromMilliseconds(kTestResults[i][j]));
    }
  }
}

TEST_F(CaptureSchedulerTest, RollingAverageDifferentTimes) {
  const int kTestResults[][arraysize(kTestInputs)] = {
    { 360, 290, 233, 133, 80, 80, 133, 233 }, // One core.
    { 180, 145, 116, 66, 50, 50, 66, 116 },   // Two cores.
    { 90, 72, 58, 50, 50, 50, 50, 58 },       // Four cores.
    { 50, 50, 50, 50, 50, 50, 50, 50 }        // Eight cores.
  };

  for (size_t i = 0; i < arraysize(kTestResults); ++i) {
    InitScheduler();
    scheduler_->SetNumOfProcessorsForTest(1 << i);
    for (size_t j = 0; j < arraysize(kTestInputs); ++j) {
      SimulateSingleFrameCapture(
          base::TimeDelta::FromMilliseconds(kTestInputs[j]),
          base::TimeDelta::FromMilliseconds(
              kTestInputs[arraysize(kTestInputs) - 1 - j]),
          base::TimeDelta::FromMilliseconds(kTestResults[i][j]));
    }
  }
}

// Verify that we never have more than 2 pending frames.
TEST_F(CaptureSchedulerTest, MaximumPendingFrames) {
  InitScheduler();

  capture_timer_->Fire();
  CheckCaptureCalled();
  scheduler_->OnCaptureCompleted();

  capture_timer_->Fire();
  CheckCaptureCalled();
  scheduler_->OnCaptureCompleted();

  EXPECT_FALSE(capture_timer_->IsRunning());

  scheduler_->OnFrameEncoded(base::TimeDelta());
  scheduler_->OnFrameSent();

  EXPECT_TRUE(capture_timer_->IsRunning());
}

}  // namespace remoting
