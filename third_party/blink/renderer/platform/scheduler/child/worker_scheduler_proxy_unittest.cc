// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/child/worker_scheduler_proxy.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/frame_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.h"
#include "third_party/blink/renderer/platform/scheduler/test/task_queue_manager_for_test.h"
#include "third_party/blink/renderer/platform/scheduler/worker/worker_thread_scheduler.h"
#include "third_party/blink/renderer/platform/waitable_event.h"

namespace blink {
namespace scheduler {

namespace {

class WorkerThreadSchedulerForTest : public WorkerThreadScheduler {
 public:
  WorkerThreadSchedulerForTest(std::unique_ptr<TaskQueueManager> manager,
                               WorkerSchedulerProxy* proxy,
                               WaitableEvent* throtting_state_changed)
      : WorkerThreadScheduler(WebThreadType::kTestThread,
                              std::move(manager),
                              proxy),
        throtting_state_changed_(throtting_state_changed) {}

  void OnThrottlingStateChanged(
      FrameScheduler::ThrottlingState throttling_state) override {
    WorkerThreadScheduler::OnThrottlingStateChanged(throttling_state);

    throtting_state_changed_->Signal();
  }

  using WorkerThreadScheduler::throttling_state;

 private:
  WaitableEvent* throtting_state_changed_;
};

class WebThreadImplForWorkerSchedulerForTest
    : public WebThreadImplForWorkerScheduler {
 public:
  WebThreadImplForWorkerSchedulerForTest(FrameScheduler* frame_scheduler,
                                         WaitableEvent* throtting_state_changed)
      : WebThreadImplForWorkerScheduler(
            WebThreadCreationParams(WebThreadType::kTestThread)
                .SetFrameScheduler(frame_scheduler)),
        throtting_state_changed_(throtting_state_changed) {}

  std::unique_ptr<NonMainThreadScheduler> CreateNonMainThreadScheduler()
      override {
    auto scheduler = std::make_unique<WorkerThreadSchedulerForTest>(
        TaskQueueManager::TakeOverCurrentThread(), worker_scheduler_proxy(),
        throtting_state_changed_);
    scheduler_ = scheduler.get();
    return scheduler;
  }

  WorkerThreadSchedulerForTest* GetWorkerScheduler() { return scheduler_; }

 private:
  WaitableEvent* throtting_state_changed_;             // NOT OWNED
  WorkerThreadSchedulerForTest* scheduler_ = nullptr;  // NOT OWNED
};

std::unique_ptr<WebThreadImplForWorkerSchedulerForTest> CreateWorkerThread(
    FrameScheduler* frame_scheduler,
    WaitableEvent* throtting_state_changed) {
  std::unique_ptr<WebThreadImplForWorkerSchedulerForTest> thread =
      std::make_unique<WebThreadImplForWorkerSchedulerForTest>(
          frame_scheduler, throtting_state_changed);
  thread->Init();
  return thread;
}

}  // namespace

class WorkerSchedulerProxyTest : public testing::Test {
 public:
  WorkerSchedulerProxyTest()
      : mock_main_thread_task_runner_(
            new cc::OrderedSimpleTaskRunner(&clock_, true)),
        main_thread_scheduler_(std::make_unique<MainThreadSchedulerImpl>(
            TaskQueueManagerForTest::Create(nullptr,
                                            mock_main_thread_task_runner_,
                                            &clock_),
            base::nullopt)),
        page_scheduler_(
            std::make_unique<PageSchedulerImpl>(nullptr,
                                                main_thread_scheduler_.get(),
                                                false)),
        frame_scheduler_(page_scheduler_->CreateFrameSchedulerImpl(
            nullptr,
            FrameScheduler::FrameType::kMainFrame)) {}

  ~WorkerSchedulerProxyTest() {
    frame_scheduler_.reset();
    page_scheduler_.reset();
    main_thread_scheduler_->Shutdown();
  }

 protected:
  base::SimpleTestTickClock clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_main_thread_task_runner_;

  std::unique_ptr<MainThreadSchedulerImpl> main_thread_scheduler_;
  std::unique_ptr<PageSchedulerImpl> page_scheduler_;
  std::unique_ptr<FrameSchedulerImpl> frame_scheduler_;
};

TEST_F(WorkerSchedulerProxyTest, VisibilitySignalReceived) {
  WaitableEvent throtting_state_changed;

  auto worker_thread =
      CreateWorkerThread(frame_scheduler_.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         FrameScheduler::ThrottlingState::kNotThrottled);

  page_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         FrameScheduler::ThrottlingState::kThrottled);

  page_scheduler_->SetPageVisible(true);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         FrameScheduler::ThrottlingState::kNotThrottled);

  mock_main_thread_task_runner_->RunUntilIdle();
}

// Tests below check that no crashes occur during different shutdown sequences.

TEST_F(WorkerSchedulerProxyTest, FrameSchedulerDestroyed) {
  WaitableEvent throtting_state_changed;

  auto worker_thread =
      CreateWorkerThread(frame_scheduler_.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         FrameScheduler::ThrottlingState::kNotThrottled);

  page_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         FrameScheduler::ThrottlingState::kThrottled);

  frame_scheduler_.reset();
  mock_main_thread_task_runner_->RunUntilIdle();

  worker_thread.reset();
  mock_main_thread_task_runner_->RunUntilIdle();
}

TEST_F(WorkerSchedulerProxyTest, ThreadDestroyed) {
  WaitableEvent throtting_state_changed;

  auto worker_thread =
      CreateWorkerThread(frame_scheduler_.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         FrameScheduler::ThrottlingState::kNotThrottled);

  page_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         FrameScheduler::ThrottlingState::kThrottled);

  worker_thread.reset();
  mock_main_thread_task_runner_->RunUntilIdle();

  page_scheduler_->SetPageVisible(true);
  mock_main_thread_task_runner_->RunUntilIdle();

  frame_scheduler_.reset();
  mock_main_thread_task_runner_->RunUntilIdle();
}

}  // namespace scheduler
}  // namespace blink
