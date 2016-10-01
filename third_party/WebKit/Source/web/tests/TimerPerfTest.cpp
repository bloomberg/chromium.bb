
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/Timer.h"

#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"

namespace blink {

class TimerPerfTest : public ::testing::Test {
 public:
  void nopTask(TimerBase*) {}

  void recordStartRunTime(TimerBase*) { m_runStart = base::ThreadTicks::Now(); }

  void recordEndRunTime(TimerBase*) {
    m_runEnd = base::ThreadTicks::Now();
    base::MessageLoop::current()->QuitNow();
  }

  base::ThreadTicks m_runStart;
  base::ThreadTicks m_runEnd;
};

TEST_F(TimerPerfTest, PostAndRunTimers) {
  const int numIterations = 10000;
  Vector<std::unique_ptr<Timer<TimerPerfTest>>> timers(numIterations);
  for (int i = 0; i < numIterations; i++) {
    timers[i].reset(new Timer<TimerPerfTest>(this, &TimerPerfTest::nopTask));
  }

  Timer<TimerPerfTest> measureRunStart(this,
                                       &TimerPerfTest::recordStartRunTime);
  Timer<TimerPerfTest> measureRunEnd(this, &TimerPerfTest::recordEndRunTime);

  measureRunStart.startOneShot(0.0, BLINK_FROM_HERE);
  base::ThreadTicks postStart = base::ThreadTicks::Now();
  for (int i = 0; i < numIterations; i++) {
    timers[i]->startOneShot(0.0, BLINK_FROM_HERE);
  }
  base::ThreadTicks postEnd = base::ThreadTicks::Now();
  measureRunEnd.startOneShot(0.0, BLINK_FROM_HERE);

  testing::enterRunLoop();

  double postingTime = (postEnd - postStart).InMicroseconds();
  double postingTimeUsPerCall =
      postingTime / static_cast<double>(numIterations);
  LOG(INFO) << "TimerBase::startOneShot cost (us/call) " << postingTimeUsPerCall
            << " (total " << postingTime << " us)";
  LOG(INFO) << "Time to run " << numIterations << " trivial tasks (us) "
            << (m_runEnd - m_runStart).InMicroseconds();
}

TEST_F(TimerPerfTest, PostThenCancelTenThousandTimers) {
  const int numIterations = 10000;
  Vector<std::unique_ptr<Timer<TimerPerfTest>>> timers(numIterations);
  for (int i = 0; i < numIterations; i++) {
    timers[i].reset(new Timer<TimerPerfTest>(this, &TimerPerfTest::nopTask));
  }

  Timer<TimerPerfTest> measureRunStart(this,
                                       &TimerPerfTest::recordStartRunTime);
  Timer<TimerPerfTest> measureRunEnd(this, &TimerPerfTest::recordEndRunTime);

  measureRunStart.startOneShot(0.0, BLINK_FROM_HERE);
  base::ThreadTicks postStart = base::ThreadTicks::Now();
  for (int i = 0; i < numIterations; i++) {
    timers[i]->startOneShot(0.0, BLINK_FROM_HERE);
  }
  base::ThreadTicks postEnd = base::ThreadTicks::Now();
  measureRunEnd.startOneShot(0.0, BLINK_FROM_HERE);

  base::ThreadTicks cancelStart = base::ThreadTicks::Now();
  for (int i = 0; i < numIterations; i++) {
    timers[i]->stop();
  }
  base::ThreadTicks cancelEnd = base::ThreadTicks::Now();

  testing::enterRunLoop();

  double postingTime = (postEnd - postStart).InMicroseconds();
  double postingTimeUsPerCall =
      postingTime / static_cast<double>(numIterations);
  LOG(INFO) << "TimerBase::startOneShot cost (us/call) " << postingTimeUsPerCall
            << " (total " << postingTime << " us)";

  double cancelTime = (cancelEnd - cancelStart).InMicroseconds();
  double cancelTimeUsPerCall = cancelTime / static_cast<double>(numIterations);
  LOG(INFO) << "TimerBase::stop cost (us/call) " << cancelTimeUsPerCall
            << " (total " << cancelTime << " us)";
  LOG(INFO) << "Time to run " << numIterations << " canceled tasks (us) "
            << (m_runEnd - m_runStart).InMicroseconds();
}

}  // namespace blink
