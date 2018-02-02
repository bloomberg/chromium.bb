// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORKER_CONTROLLER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORKER_CONTROLLER_IMPL_H_

#include "platform/scheduler/base/thread_controller.h"

#include "base/cancelable_callback.h"
#include "base/debug/task_annotator.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/sequence.h"

namespace base {
class MessageLoop;
class TickClock;
}  // namespace base

namespace blink {
namespace scheduler {
namespace internal {

class PLATFORM_EXPORT ThreadControllerImpl : public ThreadController {
 public:
  ~ThreadControllerImpl() override;

  static std::unique_ptr<ThreadControllerImpl> Create(
      base::MessageLoop* message_loop,
      base::TickClock* time_source);

  // ThreadController:
  void ScheduleWork() override;
  void ScheduleDelayedWork(base::TimeDelta delay) override;
  void CancelDelayedWork() override;
  void SetSequence(Sequence* sequence) override;
  bool RunsTasksInCurrentSequence() override;
  base::TickClock* GetClock() override;
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner>) override;
  void RestoreDefaultTaskRunner() override;
  void AddNestingObserver(base::RunLoop::NestingObserver* observer) override;
  void RemoveNestingObserver(base::RunLoop::NestingObserver* observer) override;

 protected:
  ThreadControllerImpl(base::MessageLoop* message_loop,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       base::TickClock* time_source);

  // TODO(altimin): Make these const. Blocked on removing
  // lazy initialisation support.
  base::MessageLoop* message_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

 private:
  void DoWork(Sequence::WorkType work_type);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> message_loop_task_runner_;
  base::TickClock* time_source_;
  base::RepeatingClosure immediate_do_work_closure_;
  base::RepeatingClosure delayed_do_work_closure_;
  base::CancelableClosure cancelable_delayed_do_work_closure_;
  Sequence* sequence_ = nullptr;  // NOT OWNED
  base::debug::TaskAnnotator task_annotator_;

  base::WeakPtrFactory<ThreadControllerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadControllerImpl);
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_WORKER_CONTROLLER_IMPL_H_
