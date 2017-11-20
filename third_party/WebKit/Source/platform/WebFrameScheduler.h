// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameScheduler_h
#define WebFrameScheduler_h

#include "base/memory/scoped_refptr.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebScopedVirtualTimePauser.h"

#include <memory>

namespace blink {

class WebTaskRunner;
class WebViewScheduler;

class WebFrameScheduler {
 public:
  virtual ~WebFrameScheduler() {}

  // Observer type that regulates conditions to invoke callbacks.
  enum class ObserverType { kLoader };

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
    ActiveConnectionHandle() {}
    virtual ~ActiveConnectionHandle() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ActiveConnectionHandle);
  };

  // Observer interface to receive scheduling policy change events.
  class Observer {
   public:
    // Notified when throttling state is changed.
    virtual void OnThrottlingStateChanged(ThrottlingState) = 0;
  };

  // Adds an Observer instance to be notified on scheduling policy changed.
  // When an Observer is added, the initial state will be notified synchronously
  // through the Observer interface.
  virtual void AddThrottlingObserver(ObserverType, Observer*) = 0;

  // Removes an Observer instance.
  virtual void RemoveThrottlingObserver(ObserverType, Observer*) = 0;

  // The scheduler may throttle tasks associated with offscreen frames.
  virtual void SetFrameVisible(bool) = 0;
  virtual bool IsFrameVisible() const = 0;

  // Tells the scheduler that the page this frame belongs to is not visible.
  // The scheduler may throttle tasks associated with pages that are not
  // visible.
  virtual void SetPageVisible(bool) = 0;
  virtual bool IsPageVisible() const = 0;

  // Set whether this frame is suspended. Only unthrottledTaskRunner tasks are
  // allowed to run on a suspended frame.
  virtual void SetPaused(bool) = 0;

  // Notifies observers of transitioning to and from STOPPED state in
  // background.
  virtual void SetPageStopped(bool) {}

  // Set whether this frame is cross origin w.r.t. the top level frame. Cross
  // origin frames may use a different scheduling policy from same origin
  // frames.
  virtual void SetCrossOrigin(bool) = 0;
  virtual bool IsCrossOrigin() const = 0;

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

  // Returns a WebTaskRunner that is suitable with the given task type.
  virtual scoped_refptr<WebTaskRunner> GetTaskRunner(TaskType) = 0;

  // Returns the parent WebViewScheduler.
  virtual WebViewScheduler* GetWebViewScheduler() const = 0;

  // Returns a WebScopedVirtualTimePauser which can be used to vote for pausing
  // virtual time. Virtual time will be paused if any WebScopedVirtualTimePauser
  // votes to pause it, and only unpaused only if all
  // WebScopedVirtualTimePausers are either destroyed or vote to unpause.  Note
  // the WebScopedVirtualTimePauser returned by this method is initially
  // unpaused.
  virtual WebScopedVirtualTimePauser CreateWebScopedVirtualTimePauser() = 0;

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
  // use GetWebViewScheduler()->IsExemptFromBudgetBasedThrottling for
  // the status of the page.
  virtual bool IsExemptFromBudgetBasedThrottling() const = 0;
};

}  // namespace blink

#endif  // WebFrameScheduler_h
