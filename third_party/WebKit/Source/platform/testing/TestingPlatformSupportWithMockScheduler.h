// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TestingPlatformSupportWithMockScheduler_h
#define TestingPlatformSupportWithMockScheduler_h

#include <memory>
#include "base/test/simple_test_tick_clock.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebThread.h"

namespace cc {
class OrderedSimpleTaskRunner;
}

namespace blink {

namespace scheduler {
class RendererSchedulerImpl;
}

// This class adds scheduler and threading support to TestingPlatformSupport.
// See also ScopedTestingPlatformSupport to use this class correctly.
class TestingPlatformSupportWithMockScheduler : public TestingPlatformSupport {
  WTF_MAKE_NONCOPYABLE(TestingPlatformSupportWithMockScheduler);

 public:
  TestingPlatformSupportWithMockScheduler();
  explicit TestingPlatformSupportWithMockScheduler(const Config&);
  ~TestingPlatformSupportWithMockScheduler() override;

  // Platform:
  std::unique_ptr<WebThread> CreateThread(const char* name) override;
  WebThread* CurrentThread() override;

  // Runs a single task.
  void RunSingleTask();

  // Runs all currently queued immediate tasks and delayed tasks whose delay has
  // expired plus any immediate tasks that are posted as a result of running
  // those tasks.
  //
  // This function ignores future delayed tasks when deciding if the system is
  // idle.  If you need to ensure delayed tasks run, try runForPeriodSeconds()
  // instead.
  void RunUntilIdle() override;

  // Runs for |seconds| the testing clock is advanced by |seconds|.  Note real
  // time elapsed will typically much less than |seconds| because delays between
  // timers are fast forwarded.
  void RunForPeriodSeconds(double seconds);

  // Advances |m_clock| by |seconds|.
  void AdvanceClockSeconds(double seconds);

  scheduler::RendererSchedulerImpl* GetRendererScheduler() const;

  // Controls the behavior of |m_mockTaskRunner| if true, then |m_clock| will
  // be advanced to the next timer when there's no more immediate work to do.
  void SetAutoAdvanceNowToPendingTasks(bool);

 protected:
  static double GetTestTime();

  base::SimpleTestTickClock clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_task_runner_;
  std::unique_ptr<scheduler::RendererSchedulerImpl> scheduler_;
  std::unique_ptr<WebThread> thread_;
};

}  // namespace blink

#endif  // TestingPlatformSupportWithMockScheduler_h
