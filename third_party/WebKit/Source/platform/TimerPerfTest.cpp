
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/Timer.h"

#include "base/run_loop.h"
#include "base/time/time.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TimerPerfTest : public ::testing::Test {
 public:
  void NopTask(TimerBase*) {}

  void RecordStartRunTime(TimerBase*) { run_start_ = base::ThreadTicks::Now(); }

  void RecordEndRunTime(TimerBase*) {
    run_end_ = base::ThreadTicks::Now();
    base::RunLoop::QuitCurrentDeprecated();
  }

  base::ThreadTicks run_start_;
  base::ThreadTicks run_end_;
};

TEST_F(TimerPerfTest, PostAndRunTimers) {
  const int kNumIterations = 10000;
  Vector<std::unique_ptr<Timer<TimerPerfTest>>> timers(kNumIterations);
  for (int i = 0; i < kNumIterations; i++) {
    timers[i].reset(new Timer<TimerPerfTest>(this, &TimerPerfTest::NopTask));
  }

  Timer<TimerPerfTest> measure_run_start(this,
                                         &TimerPerfTest::RecordStartRunTime);
  Timer<TimerPerfTest> measure_run_end(this, &TimerPerfTest::RecordEndRunTime);

  measure_run_start.StartOneShot(0.0, BLINK_FROM_HERE);
  base::ThreadTicks post_start = base::ThreadTicks::Now();
  for (int i = 0; i < kNumIterations; i++) {
    timers[i]->StartOneShot(0.0, BLINK_FROM_HERE);
  }
  base::ThreadTicks post_end = base::ThreadTicks::Now();
  measure_run_end.StartOneShot(0.0, BLINK_FROM_HERE);

  testing::EnterRunLoop();

  double posting_time = (post_end - post_start).InMicroseconds();
  double posting_time_us_per_call =
      posting_time / static_cast<double>(kNumIterations);
  LOG(INFO) << "TimerBase::startOneShot cost (us/call) "
            << posting_time_us_per_call << " (total " << posting_time << " us)";
  LOG(INFO) << "Time to run " << kNumIterations << " trivial tasks (us) "
            << (run_end_ - run_start_).InMicroseconds();
}

TEST_F(TimerPerfTest, PostThenCancelTenThousandTimers) {
  const int kNumIterations = 10000;
  Vector<std::unique_ptr<Timer<TimerPerfTest>>> timers(kNumIterations);
  for (int i = 0; i < kNumIterations; i++) {
    timers[i].reset(new Timer<TimerPerfTest>(this, &TimerPerfTest::NopTask));
  }

  Timer<TimerPerfTest> measure_run_start(this,
                                         &TimerPerfTest::RecordStartRunTime);
  Timer<TimerPerfTest> measure_run_end(this, &TimerPerfTest::RecordEndRunTime);

  measure_run_start.StartOneShot(0.0, BLINK_FROM_HERE);
  base::ThreadTicks post_start = base::ThreadTicks::Now();
  for (int i = 0; i < kNumIterations; i++) {
    timers[i]->StartOneShot(0.0, BLINK_FROM_HERE);
  }
  base::ThreadTicks post_end = base::ThreadTicks::Now();
  measure_run_end.StartOneShot(0.0, BLINK_FROM_HERE);

  base::ThreadTicks cancel_start = base::ThreadTicks::Now();
  for (int i = 0; i < kNumIterations; i++) {
    timers[i]->Stop();
  }
  base::ThreadTicks cancel_end = base::ThreadTicks::Now();

  testing::EnterRunLoop();

  double posting_time = (post_end - post_start).InMicroseconds();
  double posting_time_us_per_call =
      posting_time / static_cast<double>(kNumIterations);
  LOG(INFO) << "TimerBase::startOneShot cost (us/call) "
            << posting_time_us_per_call << " (total " << posting_time << " us)";

  double cancel_time = (cancel_end - cancel_start).InMicroseconds();
  double cancel_time_us_per_call =
      cancel_time / static_cast<double>(kNumIterations);
  LOG(INFO) << "TimerBase::stop cost (us/call) " << cancel_time_us_per_call
            << " (total " << cancel_time << " us)";
  LOG(INFO) << "Time to run " << kNumIterations << " canceled tasks (us) "
            << (run_end_ - run_start_).InMicroseconds();
}

}  // namespace blink
