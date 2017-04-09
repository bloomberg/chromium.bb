// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFrameScheduler_h
#define WebFrameScheduler_h

#include "wtf/RefPtr.h"

#include <memory>

namespace blink {

class WebTaskRunner;
class WebViewScheduler;

class WebFrameScheduler {
 public:
  virtual ~WebFrameScheduler() {}

  class ActiveConnectionHandle {
   public:
    ActiveConnectionHandle() {}
    virtual ~ActiveConnectionHandle() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(ActiveConnectionHandle);
  };

  // The scheduler may throttle tasks associated with offscreen frames.
  virtual void SetFrameVisible(bool) {}

  // Tells the scheduler that the page this frame belongs to supposed to be
  // throttled (because it's not been visible for a few seconds).
  virtual void SetPageThrottled(bool) {}

  // Set whether this frame is suspended. Only unthrottledTaskRunner tasks are
  // allowed to run on a suspended frame.
  virtual void SetSuspended(bool) {}

  // Set whether this frame is cross origin w.r.t. the top level frame. Cross
  // origin frames may use a different scheduling policy from same origin
  // frames.
  virtual void SetCrossOrigin(bool) {}

  // Returns the WebTaskRunner for loading tasks.
  // WebFrameScheduler owns the returned WebTaskRunner.
  virtual RefPtr<WebTaskRunner> LoadingTaskRunner() = 0;

  // Returns the WebTaskRunner for timer tasks.
  // WebFrameScheduler owns the returned WebTaskRunner.
  virtual RefPtr<WebTaskRunner> TimerTaskRunner() = 0;

  // Returns the WebTaskRunner for tasks which shouldn't get throttled,
  // but can be suspended.
  // TODO(altimin): This is a transitional task runner. Unthrottled task runner
  // would become suspendable in the nearest future and a new unsuspended
  // task runner will be added.
  virtual RefPtr<WebTaskRunner> SuspendableTaskRunner() = 0;

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
