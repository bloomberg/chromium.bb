// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_THREAD_CONTROLLER_IMPL_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_THREAD_CONTROLLER_IMPL_H_

#include "platform/scheduler/base/thread_controller.h"

#include "base/cancelable_callback.h"
#include "base/debug/task_annotator.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "platform/PlatformExport.h"
#include "platform/scheduler/base/sequenced_task_source.h"

namespace base {
class MessageLoop;
class TickClock;
}  // namespace base

namespace blink {
namespace scheduler {
namespace internal {

class PLATFORM_EXPORT ThreadControllerImpl
    : public ThreadController,
      public base::RunLoop::NestingObserver {
 public:
  ~ThreadControllerImpl() override;

  static std::unique_ptr<ThreadControllerImpl> Create(
      base::MessageLoop* message_loop,
      base::TickClock* time_source);

  // ThreadController:
  void SetWorkBatchSize(int work_batch_size) override;
  void DidQueueTask(const base::PendingTask& pending_task) override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(base::TimeTicks now,
                           base::TimeTicks run_timy) override;
  void CancelDelayedWork(base::TimeTicks run_time) override;
  void SetSequencedTaskSource(SequencedTaskSource* sequence) override;
  bool RunsTasksInCurrentSequence() override;
  base::TickClock* GetClock() override;
  void SetDefaultTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner>) override;
  void RestoreDefaultTaskRunner() override;
  void AddNestingObserver(base::RunLoop::NestingObserver* observer) override;
  void RemoveNestingObserver(base::RunLoop::NestingObserver* observer) override;

  // base::RunLoop::NestingObserver:
  void OnBeginNestedRunLoop() override;
  void OnExitNestedRunLoop() override;

 protected:
  ThreadControllerImpl(base::MessageLoop* message_loop,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                       base::TickClock* time_source);

  // TODO(altimin): Make these const. Blocked on removing
  // lazy initialisation support.
  base::MessageLoop* message_loop_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::RunLoop::NestingObserver* nesting_observer_ = nullptr;

 private:
  void DoWork(SequencedTaskSource::WorkType work_type);

  struct AnySequence {
    AnySequence() = default;

    int do_work_running_count = 0;
    int nesting_depth = 0;
    bool immediate_do_work_posted = false;
  };

  mutable base::Lock any_sequence_lock_;
  AnySequence any_sequence_;

  struct AnySequence& any_sequence() {
    any_sequence_lock_.AssertAcquired();
    return any_sequence_;
  }
  const struct AnySequence& any_sequence() const {
    any_sequence_lock_.AssertAcquired();
    return any_sequence_;
  }

  struct MainSequenceOnly {
    MainSequenceOnly() = default;

    int do_work_running_count = 0;
    int nesting_depth = 0;
    int work_batch_size_ = 1;

    base::TimeTicks next_delayed_do_work = base::TimeTicks::Max();
  };

  SEQUENCE_CHECKER(sequence_checker_);
  MainSequenceOnly main_sequence_only_;
  MainSequenceOnly& main_sequence_only() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return main_sequence_only_;
  }
  const MainSequenceOnly& main_sequence_only() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return main_sequence_only_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> message_loop_task_runner_;
  base::TickClock* time_source_;
  base::RepeatingClosure immediate_do_work_closure_;
  base::RepeatingClosure delayed_do_work_closure_;
  base::CancelableClosure cancelable_delayed_do_work_closure_;
  SequencedTaskSource* sequence_ = nullptr;  // NOT OWNED
  base::debug::TaskAnnotator task_annotator_;

  base::WeakPtrFactory<ThreadControllerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ThreadControllerImpl);
};

}  // namespace internal
}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_BASE_THREAD_CONTROLLER_IMPL_H_
