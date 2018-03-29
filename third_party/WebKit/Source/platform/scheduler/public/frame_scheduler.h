// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_PUBLIC_FRAME_SCHEDULER_H_
#define THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_PUBLIC_FRAME_SCHEDULER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebScopedVirtualTimePauser.h"

namespace blink {

class PageScheduler;

class FrameScheduler {
 public:
  virtual ~FrameScheduler() = default;

  // Observer type that regulates conditions to invoke callbacks.
  enum class ObserverType { kLoader, kWorkerScheduler };

  // Represents throttling state.
  enum class ThrottlingState {
    kThrottled,
    kNotThrottled,
    kStopped,
  };

  // Represents the type of frame: main (top-level) vs not.
  enum class FrameType {
    kMainFrame,
    kSubframe,
  };

  class ActiveConnectionHandle {
   public:
    ActiveConnectionHandle() = default;
    virtual ~ActiveConnectionHandle() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(ActiveConnectionHandle);
  };

  // Observer interface to receive scheduling policy change events.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Notified when throttling state is changed.
    virtual void OnThrottlingStateChanged(ThrottlingState) = 0;
  };

  class ThrottlingObserverHandle {
   public:
    ThrottlingObserverHandle() = default;
    virtual ~ThrottlingObserverHandle() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(ThrottlingObserverHandle);
  };

  // Adds an Observer instance to be notified on scheduling policy changed.
  // When an Observer is added, the initial state will be notified synchronously
  // through the Observer interface.
  // A RAII handle is returned and observer is unregistered when the handle is
  // destroyed.
  virtual std::unique_ptr<ThrottlingObserverHandle> AddThrottlingObserver(
      ObserverType,
      Observer*) = 0;

  // The scheduler may throttle tasks associated with offscreen frames.
  virtual void SetFrameVisible(bool) = 0;
  virtual bool IsFrameVisible() const = 0;

  // Query the page visibility state for the page associated with this frame.
  // The scheduler may throttle tasks associated with pages that are not
  // visible.
  virtual bool IsPageVisible() const = 0;

  // Set whether this frame is suspended. Only unthrottledTaskRunner tasks are
  // allowed to run on a suspended frame.
  virtual void SetPaused(bool) = 0;

  // Notifies observers of transitioning to and from FROZEN state in
  // background.
  virtual void SetPageFrozen(bool) {}

  // Set whether this frame is cross origin w.r.t. the top level frame. Cross
  // origin frames may use a different scheduling policy from same origin
  // frames.
  virtual void SetCrossOrigin(bool) = 0;
  virtual bool IsCrossOrigin() const = 0;
  virtual void TraceUrlChange(const String&) = 0;

  // Returns the frame type, which currently determines whether this frame is
  // the top level frame, i.e. a main frame.
  virtual FrameType GetFrameType() const = 0;

  // The tasks runners below are listed in increasing QoS order.
  // - throttleable task queue. Designed for custom user-provided javascript
  //   tasks. Lowest guarantees. Can be paused, blocked during user gesture,
  //   throttled when backgrounded or stopped completely after some time in
  //   background.
  // - deferrable task queue. These tasks can be deferred for a small period
  //   (several seconds) when high-priority work is anticipated. These tasks
  //   can be paused.
  // - pausable task queue. Default queue for high-priority javascript tasks.
  //   They can be paused according to the spec during javascript alert
  //   dialogs, printing windows and devtools debugging. Otherwise scheduler
  //   does not tamper with their execution.
  // - unpausable task queue. Should be used for control tasks which should
  //   run when the context is paused. Usage should be extremely rare.
  //   Please consult scheduler-dev@ before using it. Running javascript
  //   on it is strictly verboten and can lead to hard-to-diagnose errors.
  //
  //
  // These queues below are separate due to special handling for their
  // priorities.
  // - loading task queue. Similar to deferrable task queue. Throttling might
  //   be considered in the future.
  // - loading control task queue. Loading task queue with increased priority
  //   to run small loading tasks which schedule other loading tasks.

  // Note: old-style timer task runner corresponds to throttleable task runner
  // and unthrottled task runner corresponds to pausable task runner.

  // Returns a task runner that is suitable with the given task type.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
      TaskType) = 0;

  // Returns the parent PageScheduler.
  virtual PageScheduler* GetPageScheduler() const = 0;

  // Returns a WebScopedVirtualTimePauser which can be used to vote for pausing
  // virtual time. Virtual time will be paused if any WebScopedVirtualTimePauser
  // votes to pause it, and only unpaused only if all
  // WebScopedVirtualTimePausers are either destroyed or vote to unpause.  Note
  // the WebScopedVirtualTimePauser returned by this method is initially
  // unpaused.
  virtual WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser(
      WebScopedVirtualTimePauser::VirtualTaskDuration) = 0;

  // Tells the scheduler that a provisional load has started, the scheduler may
  // reset the task cost estimators and the UserModel. Must be called from the
  // main thread.
  virtual void DidStartProvisionalLoad(bool is_main_frame) = 0;

  // Tells the scheduler that a provisional load has committed, the scheduler
  // may reset the task cost estimators and the UserModel. Must be called from
  // the main thread.
  virtual void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                        bool is_reload,
                                        bool is_main_frame) = 0;

  // Tells the scheduler that the first meaningful paint has occured for this
  // frame.
  virtual void OnFirstMeaningfulPaint() = 0;

  // Notifies scheduler that this frame has established an active real time
  // connection (websocket, webrtc, etc). When connection is closed this handle
  // must be destroyed.
  virtual std::unique_ptr<ActiveConnectionHandle>
  OnActiveConnectionCreated() = 0;

  // Returns true if this frame is should not throttled (e.g. due to an active
  // connection).
  // Note that this only applies to the current frame,
  // use GetPageScheduler()->IsExemptFromBudgetBasedThrottling for the status
  // of the page.
  virtual bool IsExemptFromBudgetBasedThrottling() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBKIT_SOURCE_PLATFORM_SCHEDULER_PUBLIC_FRAME_SCHEDULER_H_
