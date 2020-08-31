// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MOUSE_LOCK_DISPATCHER_H_
#define CONTENT_RENDERER_MOUSE_LOCK_DISPATCHER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/input/input_handler.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/web/web_widget_client.h"

namespace blink {
class WebMouseEvent;
class WebLocalFrame;
}  // namespace blink

namespace content {

class CONTENT_EXPORT MouseLockDispatcher {
 public:
  MouseLockDispatcher();
  virtual ~MouseLockDispatcher();

  class LockTarget {
   public:
    virtual ~LockTarget() {}
    // A mouse lock request was pending and this reports success or failure.
    virtual void OnLockMouseACK(bool succeeded) = 0;
    // A mouse lock was in place, but has been lost.
    virtual void OnMouseLockLost() = 0;
    // A mouse lock is enabled and mouse events are being delievered.
    virtual bool HandleMouseLockedInputEvent(
        const blink::WebMouseEvent& event) = 0;
  };

  // Locks the mouse to |target| if |requester_frame| has transient user
  // activation. If true is returned, an asynchronous response to
  // target->OnLockMouseACK() will follow.
  bool LockMouse(LockTarget* target,
                 blink::WebLocalFrame* requester_frame,
                 blink::WebWidgetClient::PointerLockCallback callback,
                 bool request_unadjusted_movement);
  bool ChangeMouseLock(LockTarget* target,
                       blink::WebLocalFrame* requester_frame,
                       blink::WebWidgetClient::PointerLockCallback callback,
                       bool request_unadjusted_movement);
  // Request to unlock the mouse. This call destroys the |mouse_lock_context_|.
  // A response to target->OnMouseLockLost() will follow.
  void UnlockMouse(LockTarget* target);
  // Clears out the reference to the |target| because it has or is being
  // destroyed. Unlocks if locked. The pointer will not be accessed.
  void OnLockTargetDestroyed(LockTarget* target);
  // Clears out any reference to a lock target. Unlocks if locked.
  void ClearLockTarget();
  bool IsMouseLockedTo(LockTarget* target);

  // Allow lock target to consumed a mouse event, if it does return true.
  bool WillHandleMouseEvent(const blink::WebMouseEvent& event);

  // Subclasses or users have to call these methods to report mouse lock events
  // from the browser.
  void OnLockMouseACK(
      blink::mojom::PointerLockResult result,
      mojo::PendingRemote<blink::mojom::PointerLockContext> context);
  void OnChangeLockAck(blink::mojom::PointerLockResult result);

  void FlushContextPipeForTesting();

 protected:
  // Subclasses must implement these methods to send mouse lock requests to the
  // browser.
  virtual void SendLockMouseRequest(blink::WebLocalFrame* requester_frame,
                                    bool request_unadjusted_movement) = 0;

 private:
  bool MouseLockedOrPendingAction() const {
    return mouse_lock_context_ || pending_lock_request_ ||
           pending_unlock_request_;
  }

  void OnMouseLockLost();

  // If both |pending_lock_request_| and |pending_unlock_request_| are true,
  // it means a lock request was sent before an unlock request and we haven't
  // received responses for them. The logic in LockMouse() makes sure that a
  // lock request won't be sent when there is a pending unlock request.
  bool pending_lock_request_;
  bool pending_unlock_request_;
  mojo::Remote<blink::mojom::PointerLockContext> mouse_lock_context_;

  blink::WebWidgetClient::PointerLockCallback lock_mouse_callback_;

  // |target_| is the pending or current owner of mouse lock. We retain a non
  // owning reference here that must be cleared by |OnLockTargetDestroyed|
  // when it is destroyed.
  LockTarget* target_;

  DISALLOW_COPY_AND_ASSIGN(MouseLockDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MOUSE_LOCK_DISPATCHER_H_
