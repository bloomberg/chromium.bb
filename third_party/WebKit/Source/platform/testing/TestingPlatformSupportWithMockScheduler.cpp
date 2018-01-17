// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/TestingPlatformSupportWithMockScheduler.h"

#include "base/bind.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/WaitableEvent.h"
#include "platform/scheduler/base/real_time_domain.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/test/create_task_queue_manager_for_test.h"
#include "platform/wtf/ThreadSpecific.h"
#include "public/platform/scheduler/child/webthread_base.h"

namespace blink {

namespace {

struct ThreadLocalStorage {
  WebThread* current_thread = nullptr;
};

ThreadLocalStorage* GetThreadLocalStorage() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<ThreadLocalStorage>, tls, ());
  return tls;
}

void PrepareCurrentThread(WaitableEvent* event, WebThread* thread) {
  GetThreadLocalStorage()->current_thread = thread;
  event->Signal();
}

}  // namespace

TestingPlatformSupportWithMockScheduler::
    TestingPlatformSupportWithMockScheduler()
    : TestingPlatformSupportWithMockScheduler(
          TestingPlatformSupport::Config()) {}

TestingPlatformSupportWithMockScheduler::
    TestingPlatformSupportWithMockScheduler(const Config& config)
    : TestingPlatformSupport(config),
      mock_task_runner_(new cc::OrderedSimpleTaskRunner(&clock_, true)),
      scheduler_(new scheduler::RendererSchedulerImpl(
          scheduler::CreateTaskQueueManagerForTest(nullptr,
                                                   mock_task_runner_,
                                                   &clock_))),
      thread_(scheduler_->CreateMainThread()) {
  DCHECK(IsMainThread());
  // Set the work batch size to one so RunPendingTasks behaves as expected.
  scheduler_->GetSchedulerHelperForTesting()->SetWorkBatchSizeForTesting(1);

  WTF::SetTimeFunctionsForTesting(GetTestTime);
}

TestingPlatformSupportWithMockScheduler::
    ~TestingPlatformSupportWithMockScheduler() {
  WTF::SetTimeFunctionsForTesting(nullptr);
  scheduler_->Shutdown();
}

std::unique_ptr<WebThread>
TestingPlatformSupportWithMockScheduler::CreateThread(const char* name) {
  std::unique_ptr<scheduler::WebThreadBase> thread =
      scheduler::WebThreadBase::CreateWorkerThread(name,
                                                   base::Thread::Options());
  thread->Init();
  WaitableEvent event;
  thread->GetSingleThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(PrepareCurrentThread, base::Unretained(&event),
                                base::Unretained(thread.get())));
  event.Wait();
  return std::move(thread);
}

WebThread* TestingPlatformSupportWithMockScheduler::CurrentThread() {
  DCHECK_EQ(thread_->IsCurrentThread(), IsMainThread());

  if (thread_->IsCurrentThread()) {
    return thread_.get();
  }
  ThreadLocalStorage* storage = GetThreadLocalStorage();
  DCHECK(storage->current_thread);
  return storage->current_thread;
}

void TestingPlatformSupportWithMockScheduler::RunSingleTask() {
  mock_task_runner_->SetRunTaskLimit(1);
  mock_task_runner_->RunPendingTasks();
  mock_task_runner_->ClearRunTaskLimit();
}

void TestingPlatformSupportWithMockScheduler::RunUntilIdle() {
  mock_task_runner_->RunUntilIdle();
}

void TestingPlatformSupportWithMockScheduler::RunForPeriodSeconds(
    double seconds) {
  const base::TimeTicks deadline =
      clock_.NowTicks() + base::TimeDelta::FromSecondsD(seconds);

  scheduler::TaskQueueManager* task_queue_manager =
      scheduler_->GetSchedulerHelperForTesting()
          ->GetTaskQueueManagerForTesting();

  for (;;) {
    // If we've run out of immediate work then fast forward to the next delayed
    // task, but don't pass |deadline|.
    if (!task_queue_manager->HasImmediateWorkForTesting()) {
      base::TimeTicks next_delayed_task;
      if (!task_queue_manager->real_time_domain()->NextScheduledRunTime(
              &next_delayed_task) ||
          next_delayed_task > deadline) {
        break;
      }

      clock_.SetNowTicks(next_delayed_task);
    }

    if (clock_.NowTicks() > deadline)
      break;

    mock_task_runner_->RunPendingTasks();
  }

  clock_.SetNowTicks(deadline);
}

void TestingPlatformSupportWithMockScheduler::AdvanceClockSeconds(
    double seconds) {
  clock_.Advance(base::TimeDelta::FromSecondsD(seconds));
}

void TestingPlatformSupportWithMockScheduler::SetAutoAdvanceNowToPendingTasks(
    bool auto_advance) {
  mock_task_runner_->SetAutoAdvanceNowToPendingTasks(auto_advance);
}

scheduler::RendererSchedulerImpl*
TestingPlatformSupportWithMockScheduler::GetRendererScheduler() const {
  return scheduler_.get();
}

// static
double TestingPlatformSupportWithMockScheduler::GetTestTime() {
  TestingPlatformSupportWithMockScheduler* platform =
      static_cast<TestingPlatformSupportWithMockScheduler*>(
          Platform::Current());
  return (platform->clock_.NowTicks() - base::TimeTicks()).InSecondsF();
}

}  // namespace blink
