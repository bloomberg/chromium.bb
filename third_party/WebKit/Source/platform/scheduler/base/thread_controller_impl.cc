// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/base/thread_controller_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/time/tick_clock.h"
#include "platform/scheduler/base/sequence.h"

namespace blink {
namespace scheduler {
namespace internal {

ThreadControllerImpl::ThreadControllerImpl(
    base::MessageLoop* message_loop,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<base::TickClock> time_source)
    : message_loop_(message_loop),
      task_runner_(task_runner),
      message_loop_task_runner_(message_loop ? message_loop->task_runner()
                                             : nullptr),
      time_source_(std::move(time_source)),
      weak_factory_(this) {
  immediate_do_work_closure_ = base::BindRepeating(
      &ThreadControllerImpl::DoWork, weak_factory_.GetWeakPtr(),
      Sequence::WorkType::kImmediate);
  delayed_do_work_closure_ = base::BindRepeating(&ThreadControllerImpl::DoWork,
                                                 weak_factory_.GetWeakPtr(),
                                                 Sequence::WorkType::kDelayed);
}

ThreadControllerImpl::~ThreadControllerImpl() {}

std::unique_ptr<ThreadControllerImpl> ThreadControllerImpl::Create(
    base::MessageLoop* message_loop,
    std::unique_ptr<base::TickClock> time_source) {
  return base::WrapUnique(new ThreadControllerImpl(
      message_loop, message_loop->task_runner(), std::move(time_source)));
}

void ThreadControllerImpl::SetSequence(Sequence* sequence) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sequence);
  DCHECK(!sequence_);
  sequence_ = sequence;
}

void ThreadControllerImpl::ScheduleWork() {
  DCHECK(sequence_);
  task_runner_->PostTask(FROM_HERE, immediate_do_work_closure_);
}

void ThreadControllerImpl::ScheduleDelayedWork(base::TimeDelta delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sequence_);
  cancelable_delayed_do_work_closure_.Reset(delayed_do_work_closure_);
  task_runner_->PostDelayedTask(
      FROM_HERE, cancelable_delayed_do_work_closure_.callback(), delay);
}

void ThreadControllerImpl::CancelDelayedWork() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sequence_);
  cancelable_delayed_do_work_closure_.Cancel();
}

void ThreadControllerImpl::PostNonNestableTask(const base::Location& from_here,
                                               base::OnceClosure task) {
  task_runner_->PostNonNestableTask(from_here, std::move(task));
}

bool ThreadControllerImpl::RunsTasksInCurrentSequence() {
  return task_runner_->RunsTasksInCurrentSequence();
}

base::TickClock* ThreadControllerImpl::GetClock() {
  return time_source_.get();
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

bool ThreadControllerImpl::IsNested() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::RunLoop::IsNestedOnCurrentThread();
}

void ThreadControllerImpl::DoWork(Sequence::WorkType work_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(sequence_);
  base::Optional<base::PendingTask> task = sequence_->TakeTask(work_type);
  if (!task)
    return;
  base::WeakPtr<ThreadControllerImpl> weak_ptr = weak_factory_.GetWeakPtr();
  task_annotator_.RunTask("ThreadControllerImpl::DoWork", &task.value());
  if (weak_ptr)
    sequence_->DidRunTask();
}

void ThreadControllerImpl::AddNestingObserver(
    base::RunLoop::NestingObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RunLoop::AddNestingObserverOnCurrentThread(observer);
}

void ThreadControllerImpl::RemoveNestingObserver(
    base::RunLoop::NestingObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::RunLoop::RemoveNestingObserverOnCurrentThread(observer);
}

}  // namespace internal
}  // namespace scheduler
}  // namespace blink
