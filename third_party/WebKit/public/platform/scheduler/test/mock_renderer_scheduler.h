// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_TEST_MOCK_RENDERER_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_TEST_MOCK_RENDERER_SCHEDULER_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "cc/output/begin_frame_args.h"
#include "public/platform/WebThread.h"
#include "public/platform/scheduler/renderer/renderer_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {
namespace scheduler {

class MockRendererScheduler : public RendererScheduler {
 public:
  MockRendererScheduler() = default;
  ~MockRendererScheduler() override = default;

  MOCK_METHOD0(CreateMainThread, std::unique_ptr<blink::WebThread>());
  MOCK_METHOD0(DefaultTaskRunner,
               scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(CompositorTaskRunner,
               scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(LoadingTaskRunner,
               scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(IdleTaskRunner,
               scoped_refptr<blink::scheduler::SingleThreadIdleTaskRunner>());
  MOCK_METHOD0(TimerTaskRunner, scoped_refptr<base::SingleThreadTaskRunner>());
  MOCK_METHOD0(NewRenderWidgetSchedulingState,
               std::unique_ptr<RenderWidgetSchedulingState>());
  MOCK_METHOD1(WillBeginFrame, void(const cc::BeginFrameArgs&));
  MOCK_METHOD0(BeginFrameNotExpectedSoon, void());
  MOCK_METHOD1(BeginMainFrameNotExpectedUntil, void(base::TimeTicks));
  MOCK_METHOD0(DidCommitFrameToCompositor, void());
  MOCK_METHOD2(DidHandleInputEventOnCompositorThread,
               void(const WebInputEvent&, InputEventState));
  MOCK_METHOD2(DidHandleInputEventOnMainThread,
               void(const WebInputEvent&, WebInputEventResult));
  MOCK_METHOD0(DidAnimateForInputOnCompositorThread, void());
  MOCK_METHOD1(SetRendererHidden, void(bool));
  MOCK_METHOD1(SetRendererBackgrounded, void(bool));
  MOCK_METHOD0(SuspendRenderer, void());
  MOCK_METHOD0(ResumeRenderer, void());
  MOCK_METHOD1(AddPendingNavigation, void(NavigatingFrameType));
  MOCK_METHOD1(RemovePendingNavigation, void(NavigatingFrameType));
  MOCK_METHOD0(OnNavigate, void());
  MOCK_METHOD0(IsHighPriorityWorkAnticipated, bool());
  MOCK_CONST_METHOD0(CanExceedIdleDeadlineIfRequired, bool());
  MOCK_METHOD0(ShouldYieldForHighPriorityWork, bool());
  MOCK_METHOD1(AddTaskObserver, void(base::MessageLoop::TaskObserver*));
  MOCK_METHOD1(RemoveTaskObserver, void(base::MessageLoop::TaskObserver*));
  MOCK_METHOD0(Shutdown, void());
  MOCK_METHOD0(SuspendTimerQueue, void());
  MOCK_METHOD0(ResumeTimerQueue, void());
  MOCK_METHOD0(VirtualTimePaused, void());
  MOCK_METHOD0(VirtualTimeResumed, void());
  MOCK_METHOD1(SetTimerQueueSuspensionWhenBackgroundedEnabled, void(bool));
  MOCK_METHOD1(SetTopLevelBlameContext, void(base::trace_event::BlameContext*));
  MOCK_METHOD1(SetRAILModeObserver, void(RAILModeObserver*));
  MOCK_METHOD1(MainThreadSeemsUnresponsive, bool(base::TimeDelta));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRendererScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_TEST_MOCK_RENDERER_SCHEDULER_H_
