// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_LAZY_THREAD_CONTROLLER_FOR_TEST_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_LAZY_THREAD_CONTROLLER_FOR_TEST_H_

#include "base/threading/platform_thread.h"
#include "platform/scheduler/base/thread_controller_impl.h"

namespace blink {
namespace scheduler {

// This class connects the scheduler to a MessageLoop, but unlike
// ThreadControllerImpl it allows the message loop to be created lazily
// after the scheduler has been brought up. This is needed in testing scenarios
// where Blink is initialized before a MessageLoop has been created.
//
// TODO(skyostil): Fix the relevant test suites and remove this class
// (crbug.com/495659).
class LazyThreadControllerForTest : public internal::ThreadControllerImpl {
 public:
  LazyThreadControllerForTest();
  ~LazyThreadControllerForTest();

  // internal::ThreadControllerImpl:
  bool IsNested() override;
  void AddNestingObserver(base::RunLoop::NestingObserver* observer) override;
  void RemoveNestingObserver(base::RunLoop::NestingObserver* observer) override;
  bool RunsTasksInCurrentSequence() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(base::TimeDelta delta) override;
  void CancelDelayedWork() override;
  void PostNonNestableTask(const base::Location& from_here,
                           base::OnceClosure task);

 private:
  bool HasMessageLoop();
  void EnsureMessageLoop();

  base::PlatformThreadRef thread_ref_;

  base::RunLoop::NestingObserver* pending_observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LazyThreadControllerForTest);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_TEST_LAZY_THREAD_CONTROLLER_FOR_TEST_H_
