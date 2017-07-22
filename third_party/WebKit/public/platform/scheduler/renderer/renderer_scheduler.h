// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "public/platform/WebCommon.h"
#include "public/platform/WebInputEventResult.h"
#include "public/platform/scheduler/child/child_scheduler.h"
#include "public/platform/scheduler/child/single_thread_idle_task_runner.h"
#include "public/platform/scheduler/renderer/render_widget_scheduling_state.h"
#include "v8/include/v8.h"

namespace base {
namespace trace_event {
class BlameContext;
}
}

namespace blink {
class WebThread;
class WebInputEvent;
}

namespace viz {
struct BeginFrameArgs;
}

namespace blink {
namespace scheduler {

class RenderWidgetSchedulingState;

class BLINK_PLATFORM_EXPORT RendererScheduler : public ChildScheduler {
 public:
  class BLINK_PLATFORM_EXPORT RAILModeObserver {
   public:
    virtual ~RAILModeObserver();
    virtual void OnRAILModeChanged(v8::RAILMode rail_mode) = 0;
  };

  ~RendererScheduler() override;
  static std::unique_ptr<RendererScheduler> Create();

  // Returns the compositor task runner.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  CompositorTaskRunner() = 0;

  // Creates a WebThread implementation for the renderer main thread.
  virtual std::unique_ptr<WebThread> CreateMainThread() = 0;

  // Returns the loading task runner.  This queue is intended for tasks related
  // to resource dispatch, foreground HTML parsing, etc...
  virtual scoped_refptr<base::SingleThreadTaskRunner> LoadingTaskRunner() = 0;

  // Returns the timer task runner.  This queue is intended for DOM Timers.
  // TODO(alexclarke): Get rid of this default timer queue.
  virtual scoped_refptr<base::SingleThreadTaskRunner> TimerTaskRunner() = 0;

  // Returns a new RenderWidgetSchedulingState.  The signals from this will be
  // used to make scheduling decisions.
  virtual std::unique_ptr<RenderWidgetSchedulingState>
  NewRenderWidgetSchedulingState() = 0;

  // Called to notify about the start of an extended period where no frames
  // need to be drawn. Must be called from the main thread.
  virtual void BeginFrameNotExpectedSoon() = 0;

  // Called to notify about the start of a period where main frames are not
  // scheduled and so short idle work can be scheduled. This will precede
  // BeginFrameNotExpectedSoon and is also called when the compositor may be
  // busy but the main thread is not.
  virtual void BeginMainFrameNotExpectedUntil(base::TimeTicks time) = 0;

  // Called to notify about the start of a new frame.  Must be called from the
  // main thread.
  virtual void WillBeginFrame(const viz::BeginFrameArgs& args) = 0;

  // Called to notify that a previously begun frame was committed. Must be
  // called from the main thread.
  virtual void DidCommitFrameToCompositor() = 0;

  // Keep RendererScheduler::InputEventStateToString in sync with this enum.
  enum class InputEventState {
    EVENT_CONSUMED_BY_COMPOSITOR,
    EVENT_FORWARDED_TO_MAIN_THREAD,
  };
  static const char* InputEventStateToString(InputEventState input_event_state);

  // Tells the scheduler that the system processed an input event. Called by the
  // compositor (impl) thread.  Note it's expected that every call to
  // DidHandleInputEventOnCompositorThread where |event_state| is
  // EVENT_FORWARDED_TO_MAIN_THREAD will be followed by a corresponding call
  // to DidHandleInputEventOnMainThread.
  virtual void DidHandleInputEventOnCompositorThread(
      const WebInputEvent& web_input_event,
      InputEventState event_state) = 0;

  // Tells the scheduler that the system processed an input event. Must be
  // called from the main thread.
  virtual void DidHandleInputEventOnMainThread(
      const WebInputEvent& web_input_event,
      WebInputEventResult result) = 0;

  // Returns the most recently reported expected queueing time, computed over
  // the past 1 second window.
  virtual base::TimeDelta MostRecentExpectedQueueingTime() = 0;

  // Tells the scheduler that the system is displaying an input animation (e.g.
  // a fling). Called by the compositor (impl) thread.
  virtual void DidAnimateForInputOnCompositorThread() = 0;

  // Tells the scheduler about the change of renderer visibility status (e.g.
  // "all widgets are hidden" condition). Used mostly for metric purposes.
  // Must be called on the main thread.
  virtual void SetRendererHidden(bool hidden) = 0;

  // Tells the scheduler about the change of renderer background status, i.e.,
  // there are no critical, user facing activities (visual, audio, etc...)
  // driven by this process. A stricter condition than |SetRendererHidden()|,
  // the process is assumed to be foregrounded when the scheduler is
  // constructed. Must be called on the main thread.
  virtual void SetRendererBackgrounded(bool backgrounded) = 0;

  // Tells the scheduler that the render process should be suspended. This can
  // only be done when the renderer is backgrounded. The renderer will be
  // automatically resumed when foregrounded.
  virtual void SuspendRenderer() = 0;

  // Tells the scheduler that the render process should be resumed. This can
  // only be done when the renderer is suspended. TabManager (in the future,
  // MemoryCoordinator) will suspend the renderer again if continuously
  // backgrounded.
  virtual void ResumeRenderer() = 0;

  enum class NavigatingFrameType { kMainFrame, kChildFrame };

  // Tells the scheduler that a navigation task is pending. While any main-frame
  // navigation tasks are pending, the scheduler will ensure that loading tasks
  // are not blocked even if they are expensive. Must be called on the main
  // thread.
  virtual void AddPendingNavigation(NavigatingFrameType type) = 0;

  // Tells the scheduler that a navigation task is no longer pending.
  // Must be called on the main thread.
  virtual void RemovePendingNavigation(NavigatingFrameType type) = 0;

  // Returns true if the scheduler has reason to believe that high priority work
  // may soon arrive on the main thread, e.g., if gesture events were observed
  // recently.
  // Must be called from the main thread.
  virtual bool IsHighPriorityWorkAnticipated() = 0;

  // Suspends the timer queues and increments the timer queue suspension count.
  // May only be called from the main thread.
  virtual void SuspendTimerQueue() = 0;

  // Decrements the timer queue suspension count and re-enables the timer queues
  // if the suspension count is zero and the current schduler policy allows it.
  virtual void ResumeTimerQueue() = 0;

  // Suspends the timer queues by inserting a fence that blocks any tasks posted
  // after this point from running. Orthogonal to SuspendTimerQueue. Care must
  // be taken when using this API to avoid fighting with the TaskQueueThrottler.
  virtual void VirtualTimePaused() = 0;

  // Removes the fence added by VirtualTimePaused allowing timers to execute
  // normally. Care must be taken when using this API to avoid fighting with the
  // TaskQueueThrottler.
  virtual void VirtualTimeResumed() = 0;

  // Sets whether to allow suspension of timers after the backgrounded signal is
  // received via SetRendererBackgrounded(true). Defaults to disabled.
  virtual void SetTimerQueueSuspensionWhenBackgroundedEnabled(bool enabled) = 0;

  // Sets the default blame context to which top level work should be
  // attributed in this renderer. |blame_context| must outlive this scheduler.
  virtual void SetTopLevelBlameContext(
      base::trace_event::BlameContext* blame_context) = 0;

  // The renderer scheduler maintains an estimated RAIL mode[1]. This observer
  // can be used to get notified when the mode changes. The observer will be
  // called on the main thread and must outlive this class.
  // [1]
  // https://developers.google.com/web/tools/chrome-devtools/profile/evaluate-performance/rail
  virtual void SetRAILModeObserver(RAILModeObserver* observer) = 0;

  // Returns whether or not the main thread appears unresponsive, based on the
  // length and frequency of recent main thread tasks. To be called from the
  // compositor thread.
  virtual bool MainThreadSeemsUnresponsive(
      base::TimeDelta main_thread_responsiveness_threshold) = 0;

 protected:
  RendererScheduler();
  DISALLOW_COPY_AND_ASSIGN(RendererScheduler);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_PUBLIC_PLATFORM_SCHEDULER_RENDERER_RENDERER_SCHEDULER_H_
