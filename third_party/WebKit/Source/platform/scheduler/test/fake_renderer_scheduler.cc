// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/scheduler/test/fake_renderer_scheduler.h"

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "public/platform/WebThread.h"

namespace blink {
namespace scheduler {

FakeRendererScheduler::FakeRendererScheduler() {}

FakeRendererScheduler::~FakeRendererScheduler() {}

std::unique_ptr<blink::WebThread> FakeRendererScheduler::CreateMainThread() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::DefaultTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::CompositorTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::LoadingTaskRunner() {
  return nullptr;
}

scoped_refptr<SingleThreadIdleTaskRunner>
FakeRendererScheduler::IdleTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::TimerTaskRunner() {
  return nullptr;
}

std::unique_ptr<RenderWidgetSchedulingState>
FakeRendererScheduler::NewRenderWidgetSchedulingState() {
  return nullptr;
}

void FakeRendererScheduler::WillBeginFrame(const cc::BeginFrameArgs& args) {}

void FakeRendererScheduler::BeginFrameNotExpectedSoon() {}

void FakeRendererScheduler::BeginMainFrameNotExpectedUntil(
    base::TimeTicks time) {}

void FakeRendererScheduler::DidCommitFrameToCompositor() {}

void FakeRendererScheduler::DidHandleInputEventOnCompositorThread(
    const blink::WebInputEvent& web_input_event,
    InputEventState event_state) {}

void FakeRendererScheduler::DidHandleInputEventOnMainThread(
    const blink::WebInputEvent& web_input_event,
    WebInputEventResult result) {}

void FakeRendererScheduler::DidAnimateForInputOnCompositorThread() {}

bool FakeRendererScheduler::IsHighPriorityWorkAnticipated() {
  return false;
}

void FakeRendererScheduler::SetRendererHidden(bool hidden) {}

void FakeRendererScheduler::SetRendererBackgrounded(bool backgrounded) {}

void FakeRendererScheduler::SuspendRenderer() {}

void FakeRendererScheduler::ResumeRenderer() {}

void FakeRendererScheduler::AddPendingNavigation(NavigatingFrameType type) {}

void FakeRendererScheduler::RemovePendingNavigation(NavigatingFrameType type) {}

void FakeRendererScheduler::OnNavigate() {}

bool FakeRendererScheduler::ShouldYieldForHighPriorityWork() {
  return false;
}

bool FakeRendererScheduler::CanExceedIdleDeadlineIfRequired() const {
  return false;
}

void FakeRendererScheduler::AddTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {}

void FakeRendererScheduler::RemoveTaskObserver(
    base::MessageLoop::TaskObserver* task_observer) {}

void FakeRendererScheduler::Shutdown() {}

void FakeRendererScheduler::SuspendTimerQueue() {}

void FakeRendererScheduler::ResumeTimerQueue() {}

void FakeRendererScheduler::VirtualTimePaused() {}

void FakeRendererScheduler::VirtualTimeResumed() {}

void FakeRendererScheduler::SetTimerQueueSuspensionWhenBackgroundedEnabled(
    bool enabled) {}

void FakeRendererScheduler::SetTopLevelBlameContext(
    base::trace_event::BlameContext* blame_context) {}

void FakeRendererScheduler::SetRAILModeObserver(RAILModeObserver* observer) {}

bool FakeRendererScheduler::MainThreadSeemsUnresponsive(
    base::TimeDelta main_thread_responsiveness_threshold) {
  return false;
}

}  // namespace scheduler
}  // namespace blink
