// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_TEST_FAKE_RENDERER_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_TEST_FAKE_RENDERER_SCHEDULER_H_

#include "base/macros.h"
#include "public/platform/scheduler/renderer/renderer_scheduler.h"

namespace blink {
namespace scheduler {

class FakeRendererScheduler : public RendererScheduler {
 public:
  FakeRendererScheduler();
  ~FakeRendererScheduler() override;

  // RendererScheduler implementation.
  std::unique_ptr<WebThread> CreateMainThread() override;
  scoped_refptr<TaskQueue> DefaultTaskRunner() override;
  scoped_refptr<TaskQueue> CompositorTaskRunner() override;
  scoped_refptr<TaskQueue> LoadingTaskRunner() override;
  scoped_refptr<SingleThreadIdleTaskRunner> IdleTaskRunner() override;
  scoped_refptr<TaskQueue> TimerTaskRunner() override;
  scoped_refptr<TaskQueue> NewLoadingTaskRunner(
      TaskQueue::QueueType queue_type) override;
  scoped_refptr<TaskQueue> NewTimerTaskRunner(
      TaskQueue::QueueType queue_type) override;
  scoped_refptr<TaskQueue> NewUnthrottledTaskRunner(
      TaskQueue::QueueType queue_type) override;
  std::unique_ptr<RenderWidgetSchedulingState> NewRenderWidgetSchedulingState()
      override;
  void WillBeginFrame(const cc::BeginFrameArgs& args) override;
  void BeginFrameNotExpectedSoon() override;
  void DidCommitFrameToCompositor() override;
  void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state) override;
  void DidHandleInputEventOnMainThread(const WebInputEvent& web_input_event,
                                       WebInputEventResult result) override;
  void DidAnimateForInputOnCompositorThread() override;
  void OnRendererBackgrounded() override;
  void OnRendererForegrounded() override;
  void SuspendRenderer() override;
  void ResumeRenderer() override;
  void AddPendingNavigation(WebScheduler::NavigatingFrameType type) override;
  void RemovePendingNavigation(WebScheduler::NavigatingFrameType type) override;
  void OnNavigationStarted() override;
  bool IsHighPriorityWorkAnticipated() override;
  bool CanExceedIdleDeadlineIfRequired() const override;
  bool ShouldYieldForHighPriorityWork() override;
  void AddTaskObserver(base::MessageLoop::TaskObserver* task_observer) override;
  void RemoveTaskObserver(
      base::MessageLoop::TaskObserver* task_observer) override;
  void Shutdown() override;
  void SuspendTimerQueue() override;
  void ResumeTimerQueue() override;
  void SetTimerQueueSuspensionWhenBackgroundedEnabled(bool enabled) override;
  void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) override;
  void SetRAILModeObserver(RAILModeObserver* observer) override;
  bool MainThreadSeemsUnresponsive(
      base::TimeDelta main_thread_responsiveness_threshold) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeRendererScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_TEST_FAKE_RENDERER_SCHEDULER_H_
