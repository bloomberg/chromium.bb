// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/cpp/mouse_lock.h"
#include "ppapi/cpp/point.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "remoting/protocol/input_stub.h"

namespace pp {
class ImageData;
class InputEvent;
class Instance;
}  // namespace pp

namespace remoting {

namespace protocol {
class InputStub;
} // namespace protocol

class PepperInputHandler : public pp::MouseLock {
 public:
  // |instance| must outlive |this|.
  explicit PepperInputHandler(pp::Instance* instance);
  virtual ~PepperInputHandler();

  void set_input_stub(protocol::InputStub* input_stub) {
    input_stub_ = input_stub;
  }

  bool HandleInputEvent(const pp::InputEvent& event);

  // Enables locking the mouse when the host sets a completely transparent mouse
  // cursor.
  void AllowMouseLock();

  // Called when the plugin receives or loses focus.
  void DidChangeFocus(bool has_focus);

  // Sets the mouse cursor image. Passing NULL image will lock the mouse if
  // mouse lock is enabled.
  void SetMouseCursor(scoped_ptr<pp::ImageData> image,
                      const pp::Point& hotspot);

  // Enable or disable sending mouse input when the plugin does not have input
  // focus.
  void set_send_mouse_input_when_unfocused(bool send) {
    send_mouse_input_when_unfocused_ = send;
  }

 private:
  enum MouseLockState {
    MouseLockDisallowed,
    MouseLockOff,
    MouseLockRequestPending,
    MouseLockOn,
    MouseLockCancelling
  };

  // pp::MouseLock interface.
  virtual void MouseLockLost() OVERRIDE;

  // Requests the browser to lock the mouse and hides the cursor.
  void RequestMouseLock();

  // Requests the browser to cancel mouse lock and restores the cursor once
  // the lock is gone.
  void CancelMouseLock();

  // Applies |cursor_image_| as the custom pointer or uses the standard arrow
  // pointer if |cursor_image_| is not available.
  void UpdateMouseCursor();

  // Handles completion of the mouse lock request issued by RequestMouseLock().
  void OnMouseLocked(int error);

  pp::Instance* instance_;
  protocol::InputStub* input_stub_;

  pp::CompletionCallbackFactory<PepperInputHandler> callback_factory_;

  // Custom cursor image sent by the host. |cursor_image_| is set to NULL when
  // the cursor image is completely transparent. This can be interpreted as
  // a mouse lock request if enabled by the webapp.
  scoped_ptr<pp::ImageData> cursor_image_;

  // Hot spot for |cursor_image_|.
  pp::Point cursor_hotspot_;

  // True if the plugin has focus.
  bool has_focus_;

  // True if the plugin should respond to mouse input even if it does not have
  // keyboard focus.
  bool send_mouse_input_when_unfocused_;

  MouseLockState mouse_lock_state_;

  // Accumulated sub-pixel and sub-tick deltas from wheel events.
  float wheel_delta_x_;
  float wheel_delta_y_;
  float wheel_ticks_x_;
  float wheel_ticks_y_;

  DISALLOW_COPY_AND_ASSIGN(PepperInputHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
