// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameScheduler_h
#define WebFrameScheduler_h

#include "platform/wtf/RefPtr.h"

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
  virtual void SetFrameVisible(bool) {}

  // Tells the scheduler that the page this frame belongs to is not visible.
  // The scheduler may throttle tasks associated with pages that are not
  // visible.
  virtual void SetPageVisible(bool) {}

  // Set whether this frame is suspended. Only unthrottledTaskRunner tasks are
  // allowed to run on a suspended frame.
  virtual void SetSuspended(bool) {}

  // Set whether this frame is cross origin w.r.t. the top level frame. Cross
  // origin frames may use a different scheduling policy from same origin
  // frames.
  virtual void SetCrossOrigin(bool) {}

  // The tasks runners below are listed in increasing QoS order.
  // - timer task queue. Designed for custom user-provided javascript tasks.
  //   Lowest guarantees. Can be suspended, blocked during user gesture or
  //   throttled when backgrounded.
  // - loading task queue. Can be suspended or blocked during user gesture.
  //   Throttling might be considered in the future.
  // - suspendable task queue. Can be suspended and blocked during user gesture,
  //   can't be throttled.
  // - unthrottled-but-blockable task queue. Can't be throttled, can't
  //   be suspended but can be blocked during user gesture.
  //   NOTE: existence of this queue is a temporary fix for scroll latency
  //   regression. All tasks should be moved from this queue to suspendable
  //   or unthrottled queues and it should be deleted.
  // - unthrottled task queue. Highest guarantees. Can't be throttled,
  //   suspended or blocked. Should be used only when necessary after
  //   consulting scheduler-dev@.

  // Returns the WebTaskRunner for timer tasks.
  // WebFrameScheduler owns the returned WebTaskRunner.
  virtual RefPtr<WebTaskRunner> TimerTaskRunner() = 0;

  // Returns the WebTaskRunner for loading tasks.
  // WebFrameScheduler owns the returned WebTaskRunner.
  virtual RefPtr<WebTaskRunner> LoadingTaskRunner() = 0;

  // Returns the WebTaskRunner for tasks which shouldn't get throttled,
  // but can be suspended.
  // TODO(altimin): This is a transitional task runner. Unthrottled task runner
  // would become suspendable in the nearest future and a new unsuspended
  // task runner will be added.
  virtual RefPtr<WebTaskRunner> SuspendableTaskRunner() = 0;

  // Retuns the WebTaskRunner for tasks which should not be suspended or
  // throttled, but should be blocked during user gesture.
  // This is a temporary task runner needed for a fix for touch latency
  // regression. All tasks from it should be moved to suspendable or
  // unthrottled task runner.
  virtual RefPtr<WebTaskRunner> UnthrottledButBlockableTaskRunner() = 0;

  // Returns the WebTaskRunner for tasks which should never get throttled.
  // This is generally used for executing internal browser tasks which should
  // never be throttled. Ideally only tasks whose performance characteristics
  // are known should be posted to this task runner; for example user
  // JavaScript is discouraged. WebFrameScheduler owns the returned
  // WebTaskRunner.
  virtual RefPtr<WebTaskRunner> UnthrottledTaskRunner() = 0;

  // Returns the parent WebViewScheduler.
  virtual WebViewScheduler* GetWebViewScheduler() { return nullptr; }

  // Tells the scheduler a resource load has started. The scheduler may make
  // policy decisions based on this.
  virtual void DidStartLoading(unsigned long identifier) {}

  // Tells the scheduler a resource load has stopped. The scheduler may make
  // policy decisions based on this.
  virtual void DidStopLoading(unsigned long identifier) {}

  // Tells the scheduler that a history navigation is expected soon, virtual
  // time may be paused. Must be called from the main thread.
  virtual void WillNavigateBackForwardSoon() {}

  // Tells the scheduler that a provisional load has started, virtual time may
  // be paused. Must be called from the main thread.
  virtual void DidStartProvisionalLoad(bool is_main_frame) {}

  // Tells the scheduler that a provisional load has failed, virtual time may be
  // unpaused. Must be called from the main thread.
  virtual void DidFailProvisionalLoad() {}

  // Tells the scheduler that a provisional load has committed, virtual time ma
  // be unpaused. In addition the scheduler may reset the task cost estimators
  // and the UserModel. Must be called from the main thread.
  virtual void DidCommitProvisionalLoad(bool is_web_history_inert_commit,
                                        bool is_reload,
                                        bool is_main_frame){};

  // Tells the scheduler if we are parsing a document on another thread. This
  // tells the scheduler not to advance virtual time if it's using the
  // DETERMINISTIC_LOADING policy.
  virtual void SetDocumentParsingInBackground(bool) {}

  // Tells the scheduler that the first meaningful paint has occured for this
  // frame.
  virtual void OnFirstMeaningfulPaint() {}

  // Notifies scheduler that this frame has established an active real time
  // connection (websocket, webrtc, etc). When connection is closed this handle
  // must be destroyed.
  virtual std::unique_ptr<ActiveConnectionHandle> OnActiveConnectionCreated() {
    return nullptr;
  };
};

}  // namespace blink

#endif  // WebFrameScheduler_h
