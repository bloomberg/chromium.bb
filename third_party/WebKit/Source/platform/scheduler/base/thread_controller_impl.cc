// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/thread_controller_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "platform/scheduler/base/lazy_now.h"

namespace blink {
namespace scheduler {
namespace internal {

ThreadControllerImpl::ThreadControllerImpl(
    base::MessageLoop* message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TickClock* time_source)
    : message_loop_(message_loop),
      task_runner_(task_runner),
      message_loop_task_runner_(message_loop ? message_loop->task_runner()
                                             : nullptr),
      time_source_(time_source),
      weak_factory_(this) {
  immediate_do_work_closure_ = base::BindRepeating(
      &ThreadControllerImpl::DoWork, weak_factory_.GetWeakPtr(),
      SequencedTaskSource::WorkType::kImmediate);
  delayed_do_work_closure_ = base::BindRepeating(
      &ThreadControllerImpl::DoWork, weak_factory_.GetWeakPtr(),
      SequencedTaskSource::WorkType::kDelayed);
}

ThreadControllerImpl::~ThreadControllerImpl() = default;

std::unique_ptr<ThreadControllerImpl> ThreadControllerImpl::Create(
    base::MessageLoop* message_loop,
    const base::TickClock* time_source) {
  return base::WrapUnique(new ThreadControllerImpl(
      message_loop, message_loop->task_runner(), time_source));
}

void ThreadControllerImpl::SetSequencedTaskSource(
    SequencedTaskSource* sequence) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sequence);
  DCHECK(!sequence_);
  sequence_ = sequence;
}

void ThreadControllerImpl::ScheduleWork() {
  DCHECK(sequence_);
  base::AutoLock lock(any_sequence_lock_);
  // Don't post a DoWork if there's an immediate DoWork in flight or if we're
  // inside a top level DoWork. We can rely on a continuation being posted as
  // needed.
  if (any_sequence().immediate_do_work_posted ||
      (any_sequence().do_work_running_count > any_sequence().nesting_depth)) {
    return;
  }
  any_sequence().immediate_do_work_posted = true;

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "ThreadControllerImpl::ScheduleWork::PostTask");
  task_runner_->PostTask(FROM_HERE, immediate_do_work_closure_);
}

void ThreadControllerImpl::ScheduleDelayedWork(base::TimeTicks now,
                                               base::TimeTicks run_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sequence_);

  // If there's a delayed DoWork scheduled to run sooner, we don't need to do
  // anything because a delayed continuation will be posted as needed.
  if (main_sequence_only().next_delayed_do_work <= run_time)
    return;

  // If DoWork is running then we don't need to do anything because it will post
  // a continuation as needed. Bailing out here is by far the most common case.
  if (main_sequence_only().do_work_running_count >
      main_sequence_only().nesting_depth) {
    return;
  }

  // If DoWork is about to run then we also don't need to do anything.
  {
    base::AutoLock lock(any_sequence_lock_);
    if (any_sequence().immediate_do_work_posted)
      return;
  }

  base::TimeDelta delay = std::max(base::TimeDelta(), run_time - now);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
               "ThreadControllerImpl::ScheduleDelayedWork::PostDelayedTask",
               "delay_ms", delay.InMillisecondsF());

  main_sequence_only().next_delayed_do_work = run_time;
  cancelable_delayed_do_work_closure_.Reset(delayed_do_work_closure_);
  task_runner_->PostDelayedTask(
      FROM_HERE, cancelable_delayed_do_work_closure_.callback(), delay);
}

void ThreadControllerImpl::CancelDelayedWork(base::TimeTicks run_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sequence_);
  if (main_sequence_only().next_delayed_do_work != run_time)
    return;

  cancelable_delayed_do_work_closure_.Cancel();
  main_sequence_only().next_delayed_do_work = base::TimeTicks::Max();
}

bool ThreadControllerImpl::RunsTasksInCurrentSequence() {
  return task_runner_->RunsTasksInCurrentSequence();
}

const base::TickClock* ThreadControllerImpl::GetClock() {
  return time_source_;
}

void ThreadControllerImpl::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!message_loop_)
    return;
  message_loop_->SetTaskRunner(task_runner);
}

void ThreadControllerImpl::RestoreDefaultTaskRunner() {
  if (!message_loop_)
    return;
  message_loop_->SetTaskRunner(message_loop_task_runner_);
}

void ThreadControllerImpl::DidQueueTask(const base::PendingTask& pending_task) {
  task_annotator_.DidQueueTask("TaskQueueManager::PostTask", pending_task);
}

void ThreadControllerImpl::DoWork(SequencedTaskSource::WorkType work_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sequence_);

  {
    base::AutoLock lock(any_sequence_lock_);
    if (work_type == SequencedTaskSource::WorkType::kImmediate)
      any_sequence().immediate_do_work_posted = false;
    any_sequence().do_work_running_count++;
  }

  main_sequence_only().do_work_running_count++;

  base::WeakPtr<ThreadControllerImpl> weak_ptr = weak_factory_.GetWeakPtr();
  // TODO(scheduler-dev): Consider moving to a time based work batch instead.
  for (int i = 0; i < main_sequence_only().work_batch_size_; i++) {
    base::Optional<base::PendingTask> task = sequence_->TakeTask();
    if (!task)
      break;

    TRACE_TASK_EXECUTION("ThreadControllerImpl::DoWork", *task);
    task_annotator_.RunTask("ThreadControllerImpl::DoWork", &*task);

    if (!weak_ptr)
      return;

    sequence_->DidRunTask();

    // TODO(alexclarke): Find out why this is needed.
    if (main_sequence_only().nesting_depth > 0)
      break;
  }

  main_sequence_only().do_work_running_count--;

  {
    base::AutoLock lock(any_sequence_lock_);
    any_sequence().do_work_running_count--;
    DCHECK_GE(any_sequence().do_work_running_count, 0);
    LazyNow lazy_now(time_source_);
    base::TimeDelta delay_till_next_task =
        sequence_->DelayTillNextTask(&lazy_now);
    if (delay_till_next_task <= base::TimeDelta()) {
      // The next task needs to run immediately, post a continuation if needed.
      if (!any_sequence().immediate_do_work_posted) {
        any_sequence().immediate_do_work_posted = true;
        task_runner_->PostTask(FROM_HERE, immediate_do_work_closure_);
      }
    } else if (delay_till_next_task < base::TimeDelta::Max()) {
      // The next task needs to run after a delay, post a continuation if
      // needed.
      base::TimeTicks next_task_at = lazy_now.Now() + delay_till_next_task;
      if (next_task_at != main_sequence_only().next_delayed_do_work) {
        main_sequence_only().next_delayed_do_work = next_task_at;
        cancelable_delayed_do_work_closure_.Reset(delayed_do_work_closure_);
        task_runner_->PostDelayedTask(
            FROM_HERE, cancelable_delayed_do_work_closure_.callback(),
            delay_till_next_task);
      }
    } else {
      // There is no next task scheduled.
      main_sequence_only().next_delayed_do_work = base::TimeTicks::Max();
    }
  }
}

void ThreadControllerImpl::AddNestingObserver(
    base::RunLoop::NestingObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  nesting_observer_ = observer;
  base::RunLoop::AddNestingObserverOnCurrentThread(this);
}

void ThreadControllerImpl::RemoveNestingObserver(
    base::RunLoop::NestingObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(observer, nesting_observer_);
  nesting_observer_ = nullptr;
  base::RunLoop::RemoveNestingObserverOnCurrentThread(this);
}

void ThreadControllerImpl::OnBeginNestedRunLoop() {
  main_sequence_only().nesting_depth++;
  {
    base::AutoLock lock(any_sequence_lock_);
    any_sequence().nesting_depth++;
    if (!any_sequence().immediate_do_work_posted) {
      any_sequence().immediate_do_work_posted = true;
      TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("renderer.scheduler"),
                   "ThreadControllerImpl::OnBeginNestedRunLoop::PostTask");
      task_runner_->PostTask(FROM_HERE, immediate_do_work_closure_);
    }
  }
  if (nesting_observer_)
    nesting_observer_->OnBeginNestedRunLoop();
}

void ThreadControllerImpl::OnExitNestedRunLoop() {
  main_sequence_only().nesting_depth--;
  {
    base::AutoLock lock(any_sequence_lock_);
    any_sequence().nesting_depth--;
    DCHECK_GE(any_sequence().nesting_depth, 0);
  }
  if (nesting_observer_)
    nesting_observer_->OnExitNestedRunLoop();
}

void ThreadControllerImpl::SetWorkBatchSize(int work_batch_size) {
  main_sequence_only().work_batch_size_ = work_batch_size;
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
