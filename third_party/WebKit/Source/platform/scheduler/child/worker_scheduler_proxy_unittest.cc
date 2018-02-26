// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/child/worker_scheduler_proxy.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/test/ordered_simple_task_runner.h"
#include "platform/WaitableEvent.h"
#include "platform/scheduler/child/webthread_impl_for_worker_scheduler.h"
#include "platform/scheduler/child/worker_scheduler_impl.h"
#include "platform/scheduler/renderer/renderer_scheduler_impl.h"
#include "platform/scheduler/renderer/web_frame_scheduler_impl.h"
#include "platform/scheduler/renderer/web_view_scheduler_impl.h"
#include "platform/scheduler/test/create_task_queue_manager_for_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

namespace {

class WorkerSchedulerImplForTest : public WorkerSchedulerImpl {
 public:
  WorkerSchedulerImplForTest(std::unique_ptr<TaskQueueManager> manager,
                             WorkerSchedulerProxy* proxy,
                             WaitableEvent* throtting_state_changed)
      : WorkerSchedulerImpl(std::move(manager), proxy),
        throtting_state_changed_(throtting_state_changed) {}

  void OnThrottlingStateChanged(
      WebFrameScheduler::ThrottlingState throttling_state) override {
    WorkerSchedulerImpl::OnThrottlingStateChanged(throttling_state);

    throtting_state_changed_->Signal();
  }

  using WorkerSchedulerImpl::throttling_state;

 private:
  WaitableEvent* throtting_state_changed_;
};

class WebThreadImplForWorkerSchedulerForTest
    : public WebThreadImplForWorkerScheduler {
 public:
  explicit WebThreadImplForWorkerSchedulerForTest(
      WorkerSchedulerProxy* proxy,
      WaitableEvent* throtting_state_changed)
      : WebThreadImplForWorkerScheduler("Test Worker", base::Thread::Options()),
        proxy_(proxy),
        throtting_state_changed_(throtting_state_changed) {}

  std::unique_ptr<WorkerScheduler> CreateWorkerScheduler() override {
    auto scheduler = std::make_unique<WorkerSchedulerImplForTest>(
        TaskQueueManager::TakeOverCurrentThread(), proxy_,
        throtting_state_changed_);
    scheduler_ = scheduler.get();
    return scheduler;
  }

  WorkerSchedulerImplForTest* GetWorkerScheduler() { return scheduler_; }

 private:
  WorkerSchedulerProxy* proxy_;                      // NOW OWNED
  WaitableEvent* throtting_state_changed_;           // NOT OWNED
  WorkerSchedulerImplForTest* scheduler_ = nullptr;  // NOT OWNED
};

std::unique_ptr<WebThreadImplForWorkerSchedulerForTest> CreateWorkerThread(
    WorkerSchedulerProxy* proxy,
    WaitableEvent* throtting_state_changed) {
  std::unique_ptr<WebThreadImplForWorkerSchedulerForTest> thread =
      std::make_unique<WebThreadImplForWorkerSchedulerForTest>(
          proxy, throtting_state_changed);
  thread->Init();
  return thread;
}

}  // namespace

class WorkerSchedulerProxyTest : public ::testing::Test {
 public:
  WorkerSchedulerProxyTest()
      : mock_main_thread_task_runner_(
            new cc::OrderedSimpleTaskRunner(&clock_, true)),
        renderer_scheduler_(std::make_unique<RendererSchedulerImpl>(
            CreateTaskQueueManagerForTest(nullptr,
                                          mock_main_thread_task_runner_,
                                          &clock_),
            base::nullopt)),
        web_view_scheduler_(
            std::make_unique<WebViewSchedulerImpl>(nullptr,
                                                   nullptr,
                                                   renderer_scheduler_.get(),
                                                   false)),
        frame_scheduler_(web_view_scheduler_->CreateWebFrameSchedulerImpl(
            nullptr,
            WebFrameScheduler::FrameType::kMainFrame)) {}

  ~WorkerSchedulerProxyTest() {
    frame_scheduler_.reset();
    web_view_scheduler_.reset();
    renderer_scheduler_->Shutdown();
  }

 protected:
  base::SimpleTestTickClock clock_;
  scoped_refptr<cc::OrderedSimpleTaskRunner> mock_main_thread_task_runner_;

  std::unique_ptr<RendererSchedulerImpl> renderer_scheduler_;
  std::unique_ptr<WebViewSchedulerImpl> web_view_scheduler_;
  std::unique_ptr<WebFrameSchedulerImpl> frame_scheduler_;
};

TEST_F(WorkerSchedulerProxyTest, VisibilitySignalReceived) {
  WaitableEvent throtting_state_changed;

  std::unique_ptr<WorkerSchedulerProxy> proxy =
      std::make_unique<WorkerSchedulerProxy>(frame_scheduler_.get());

  auto worker_thread =
      CreateWorkerThread(proxy.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         WebFrameScheduler::ThrottlingState::kNotThrottled);

  frame_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         WebFrameScheduler::ThrottlingState::kThrottled);

  frame_scheduler_->SetPageVisible(true);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         WebFrameScheduler::ThrottlingState::kNotThrottled);

  mock_main_thread_task_runner_->RunUntilIdle();
}

// Tests below check that no crashes occur during different shutdown sequences.

TEST_F(WorkerSchedulerProxyTest, FrameSchedulerDestroyed) {
  WaitableEvent throtting_state_changed;

  std::unique_ptr<WorkerSchedulerProxy> proxy =
      std::make_unique<WorkerSchedulerProxy>(frame_scheduler_.get());

  auto worker_thread =
      CreateWorkerThread(proxy.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         WebFrameScheduler::ThrottlingState::kNotThrottled);

  frame_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         WebFrameScheduler::ThrottlingState::kThrottled);

  frame_scheduler_.reset();
  mock_main_thread_task_runner_->RunUntilIdle();

  worker_thread.reset();
  proxy.reset();
  mock_main_thread_task_runner_->RunUntilIdle();
}

TEST_F(WorkerSchedulerProxyTest, ThreadDestroyed) {
  WaitableEvent throtting_state_changed;

  std::unique_ptr<WorkerSchedulerProxy> proxy =
      std::make_unique<WorkerSchedulerProxy>(frame_scheduler_.get());

  auto worker_thread =
      CreateWorkerThread(proxy.get(), &throtting_state_changed);

  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         WebFrameScheduler::ThrottlingState::kNotThrottled);

  frame_scheduler_->SetPageVisible(false);
  throtting_state_changed.Wait();
  DCHECK(worker_thread->GetWorkerScheduler()->throttling_state() ==
         WebFrameScheduler::ThrottlingState::kThrottled);

  worker_thread.reset();
  proxy.reset();
  mock_main_thread_task_runner_->RunUntilIdle();

  frame_scheduler_->SetPageVisible(true);
  mock_main_thread_task_runner_->RunUntilIdle();

  frame_scheduler_.reset();
  mock_main_thread_task_runner_->RunUntilIdle();
}

}  // namespace scheduler
}  // namespace blink
