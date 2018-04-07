// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/scheduler/test/fake_renderer_scheduler.h"

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_thread.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace scheduler {

FakeRendererScheduler::FakeRendererScheduler() = default;

FakeRendererScheduler::~FakeRendererScheduler() = default;

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
FakeRendererScheduler::InputTaskRunner() {
  return nullptr;
}

scoped_refptr<SingleThreadIdleTaskRunner>
FakeRendererScheduler::IdleTaskRunner() {
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeRendererScheduler::IPCTaskRunner() {
  return nullptr;
}

std::unique_ptr<WebRenderWidgetSchedulingState>
FakeRendererScheduler::NewRenderWidgetSchedulingState() {
  return nullptr;
}

void FakeRendererScheduler::WillBeginFrame(const viz::BeginFrameArgs& args) {}

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

base::TimeDelta FakeRendererScheduler::MostRecentExpectedQueueingTime() {
  return base::TimeDelta();
}

void FakeRendererScheduler::DidAnimateForInputOnCompositorThread() {}

bool FakeRendererScheduler::IsHighPriorityWorkAnticipated() {
  return false;
}

void FakeRendererScheduler::SetRendererHidden(bool hidden) {}

void FakeRendererScheduler::SetRendererBackgrounded(bool backgrounded) {}

void FakeRendererScheduler::SetSchedulerKeepActive(bool keep_active) {}

std::unique_ptr<FakeRendererScheduler::RendererPauseHandle>
FakeRendererScheduler::PauseRenderer() {
  return nullptr;
}

#if defined(OS_ANDROID)
void FakeRendererScheduler::PauseTimersForAndroidWebView() {}

void FakeRendererScheduler::ResumeTimersForAndroidWebView() {}
#endif

void FakeRendererScheduler::AddPendingNavigation(NavigatingFrameType type) {}

void FakeRendererScheduler::RemovePendingNavigation(NavigatingFrameType type) {}

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

void FakeRendererScheduler::SetStoppingWhenBackgroundedEnabled(bool enabled) {}

void FakeRendererScheduler::SetTopLevelBlameContext(
    base::trace_event::BlameContext* blame_context) {}

void FakeRendererScheduler::SetRAILModeObserver(RAILModeObserver* observer) {}

bool FakeRendererScheduler::MainThreadSeemsUnresponsive(
    base::TimeDelta main_thread_responsiveness_threshold) {
  return false;
}

void FakeRendererScheduler::SetRendererProcessType(RendererProcessType type) {}

WebScopedVirtualTimePauser
FakeRendererScheduler::CreateWebScopedVirtualTimePauser(
    const char* name,
    WebScopedVirtualTimePauser::VirtualTaskDuration duration) {
  return WebScopedVirtualTimePauser(nullptr, duration,
                                    WebString(WTF::String(name)));
}

}  // namespace scheduler
}  // namespace blink
