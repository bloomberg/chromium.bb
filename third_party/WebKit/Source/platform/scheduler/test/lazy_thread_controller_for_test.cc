// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/test/lazy_thread_controller_for_test.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/time/default_tick_clock.h"

namespace blink {
namespace scheduler {

LazyThreadControllerForTest::LazyThreadControllerForTest()
    : ThreadControllerImpl(base::MessageLoop::current(),
                           nullptr,
                           base::DefaultTickClock::GetInstance()),
      thread_ref_(base::PlatformThread::CurrentRef()) {
  if (message_loop_)
    task_runner_ = message_loop_->task_runner();
}

LazyThreadControllerForTest::~LazyThreadControllerForTest() {}

void LazyThreadControllerForTest::EnsureMessageLoop() {
  if (message_loop_)
    return;
  DCHECK(RunsTasksInCurrentSequence());
  message_loop_ = base::MessageLoop::current();
  DCHECK(message_loop_);
  task_runner_ = message_loop_->task_runner();
  if (pending_observer_) {
    base::RunLoop::AddNestingObserverOnCurrentThread(pending_observer_);
    pending_observer_ = nullptr;
  }
  if (pending_default_task_runner_) {
    ThreadControllerImpl::SetDefaultTaskRunner(pending_default_task_runner_);
    pending_default_task_runner_ = nullptr;
  }
}

bool LazyThreadControllerForTest::HasMessageLoop() {
  return !!message_loop_;
}

void LazyThreadControllerForTest::AddNestingObserver(
    base::RunLoop::NestingObserver* observer) {
  // While |observer| _could_ be associated with the current thread regardless
  // of the presence of a MessageLoop, the association is delayed until
  // EnsureMessageLoop() is invoked. This works around a state issue where
  // otherwise many tests fail because of the following sequence:
  //   1) blink::scheduler::CreateRendererSchedulerForTests()
  //       -> TaskQueueManager::TaskQueueManager()
  //       -> LazySchedulerMessageLoopDelegateForTests::AddNestingObserver()
  //   2) Any test framework with a base::MessageLoop member (and not caring
  //      about the blink scheduler) does:
  //        ThreadTaskRunnerHandle::Get()->PostTask(
  //            FROM_HERE, an_init_task_with_a_nested_loop);
  //        RunLoop.RunUntilIdle();
  //   3) |a_task_with_a_nested_loop| triggers
  //          TaskQueueManager::OnBeginNestedLoop() which:
  //            a) flags any_thread().is_nested = true;
  //            b) posts a task to self, which triggers:
  //                 LazySchedulerMessageLoopDelegateForTests::PostDelayedTask()
  //   4) This self-task in turn triggers TaskQueueManager::DoWork()
  //      which expects to be the only one to trigger nested loops (doesn't
  //      support TaskQueueManager::OnBeginNestedLoop() being invoked before
  //      it kicks in), resulting in it hitting:
  //      DCHECK_EQ(any_thread().is_nested, delegate_->IsNested()); (1 vs 0).
  // TODO(skyostil): fix this convolution as part of http://crbug.com/495659.
  if (!HasMessageLoop()) {
    DCHECK(!pending_observer_);
    pending_observer_ = observer;
    return;
  }
  base::RunLoop::AddNestingObserverOnCurrentThread(observer);
}

void LazyThreadControllerForTest::RemoveNestingObserver(
    base::RunLoop::NestingObserver* observer) {
  if (!HasMessageLoop()) {
    DCHECK_EQ(pending_observer_, observer);
    pending_observer_ = nullptr;
    return;
  }
  if (base::MessageLoop::current() != message_loop_)
    return;
  base::RunLoop::RemoveNestingObserverOnCurrentThread(observer);
}

bool LazyThreadControllerForTest::IsNested() {
  EnsureMessageLoop();
  return ThreadControllerImpl::IsNested();
}

bool LazyThreadControllerForTest::RunsTasksInCurrentSequence() {
  return thread_ref_ == base::PlatformThread::CurrentRef();
}

void LazyThreadControllerForTest::ScheduleWork() {
  EnsureMessageLoop();
  ThreadControllerImpl::ScheduleWork();
}

void LazyThreadControllerForTest::ScheduleDelayedWork(base::TimeDelta delay) {
  EnsureMessageLoop();
  ThreadControllerImpl::ScheduleDelayedWork(delay);
}

void LazyThreadControllerForTest::CancelDelayedWork() {
  EnsureMessageLoop();
  ThreadControllerImpl::CancelDelayedWork();
}

void LazyThreadControllerForTest::PostNonNestableTask(
    const base::Location& from_here,
    base::OnceClosure task) {
  EnsureMessageLoop();
  ThreadControllerImpl::PostNonNestableTask(from_here, std::move(task));
}

void LazyThreadControllerForTest::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!HasMessageLoop()) {
    pending_default_task_runner_ = task_runner;
    return;
  }
  ThreadControllerImpl::SetDefaultTaskRunner(task_runner);
}

void LazyThreadControllerForTest::RestoreDefaultTaskRunner() {
  pending_default_task_runner_ = nullptr;
  if (HasMessageLoop() && base::MessageLoop::current() == message_loop_)
    ThreadControllerImpl::RestoreDefaultTaskRunner();
}

}  // namespace scheduler
}  // namespace blink
