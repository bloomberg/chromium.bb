// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_input_handler.h"

#include "base/logging.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/mouse_cursor.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/var.h"
#include "remoting/proto/event.pb.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"

namespace remoting {

PepperInputHandler::PepperInputHandler(
    pp::Instance* instance)
    : pp::MouseLock(instance),
      instance_(instance),
      input_stub_(NULL),
      callback_factory_(this),
      has_focus_(false),
      send_mouse_input_when_unfocused_(false),
      mouse_lock_state_(MouseLockDisallowed),
      wheel_delta_x_(0),
      wheel_delta_y_(0),
      wheel_ticks_x_(0),
      wheel_ticks_y_(0) {
}

PepperInputHandler::~PepperInputHandler() {}

// Helper function to get the USB key code using the Dev InputEvent interface.
uint32_t GetUsbKeyCode(pp::KeyboardInputEvent pp_key_event) {
  // Get the DOM3 |code| as a string.
  std::string codestr = pp_key_event.GetCode().AsString();

  // Convert the |code| string into a USB keycode.
  ui::KeycodeConverter* key_converter = ui::KeycodeConverter::GetInstance();
  return key_converter->CodeToUsbKeycode(codestr.c_str());
}

bool PepperInputHandler::HandleInputEvent(const pp::InputEvent& event) {
  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_CONTEXTMENU: {
      // We need to return true here or else we'll get a local (plugin) context
      // menu instead of the mouseup event for the right click.
      return true;
    }

    case PP_INPUTEVENT_TYPE_KEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP: {
      pp::KeyboardInputEvent pp_key_event(event);
      uint32_t modifiers = event.GetModifiers();
      uint32_t lock_states = 0;

      if (modifiers & PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY)
        lock_states |= protocol::KeyEvent::LOCK_STATES_CAPSLOCK;

      if (modifiers & PP_INPUTEVENT_MODIFIER_NUMLOCKKEY)
        lock_states |= protocol::KeyEvent::LOCK_STATES_NUMLOCK;

      protocol::KeyEvent key_event;
      key_event.set_usb_keycode(GetUsbKeyCode(pp_key_event));
      key_event.set_pressed(event.GetType() == PP_INPUTEVENT_TYPE_KEYDOWN);
      key_event.set_lock_states(lock_states);

      if (input_stub_)
        input_stub_->InjectKeyEvent(key_event);
      return true;
    }

    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP: {
      if (!has_focus_ && !send_mouse_input_when_unfocused_)
        return false;

      pp::MouseInputEvent pp_mouse_event(event);
      protocol::MouseEvent mouse_event;
      switch (pp_mouse_event.GetButton()) {
        case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
          mouse_event.set_button(protocol::MouseEvent::BUTTON_LEFT);
          break;
        case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
          mouse_event.set_button(protocol::MouseEvent::BUTTON_MIDDLE);
          break;
        case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
          mouse_event.set_button(protocol::MouseEvent::BUTTON_RIGHT);
          break;
        case PP_INPUTEVENT_MOUSEBUTTON_NONE:
          break;
      }
      if (mouse_event.has_button()) {
        bool is_down = (event.GetType() == PP_INPUTEVENT_TYPE_MOUSEDOWN);
        mouse_event.set_button_down(is_down);
        mouse_event.set_x(pp_mouse_event.GetPosition().x());
        mouse_event.set_y(pp_mouse_event.GetPosition().y());

        // Add relative movement if the mouse is locked.
        if (mouse_lock_state_ == MouseLockOn) {
          pp::Point delta = pp_mouse_event.GetMovement();
          mouse_event.set_delta_x(delta.x());
          mouse_event.set_delta_y(delta.y());
        }

        if (input_stub_)
          input_stub_->InjectMouseEvent(mouse_event);
      }
      return true;
    }

    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE: {
      if (!has_focus_ && !send_mouse_input_when_unfocused_)
        return false;

      pp::MouseInputEvent pp_mouse_event(event);
      protocol::MouseEvent mouse_event;
      mouse_event.set_x(pp_mouse_event.GetPosition().x());
      mouse_event.set_y(pp_mouse_event.GetPosition().y());

      // Add relative movement if the mouse is locked.
      if (mouse_lock_state_ == MouseLockOn) {
        pp::Point delta = pp_mouse_event.GetMovement();
        mouse_event.set_delta_x(delta.x());
        mouse_event.set_delta_y(delta.y());
      }

      if (input_stub_)
        input_stub_->InjectMouseEvent(mouse_event);
      return true;
    }

    case PP_INPUTEVENT_TYPE_WHEEL: {
      if (!has_focus_ && !send_mouse_input_when_unfocused_)
        return false;

      pp::WheelInputEvent pp_wheel_event(event);

      // Don't handle scroll-by-page events, for now.
      if (pp_wheel_event.GetScrollByPage())
        return false;

      // Add this event to our accumulated sub-pixel deltas and clicks.
      pp::FloatPoint delta = pp_wheel_event.GetDelta();
      wheel_delta_x_ += delta.x();
      wheel_delta_y_ += delta.y();
      pp::FloatPoint ticks = pp_wheel_event.GetTicks();
      wheel_ticks_x_ += ticks.x();
      wheel_ticks_y_ += ticks.y();

      // If there is at least a pixel's movement, emit an event. We don't
      // ever expect to accumulate one tick's worth of scrolling without
      // accumulating a pixel's worth at the same time, so this is safe.
      int delta_x = static_cast<int>(wheel_delta_x_);
      int delta_y = static_cast<int>(wheel_delta_y_);
      if (delta_x != 0 || delta_y != 0) {
        wheel_delta_x_ -= delta_x;
        wheel_delta_y_ -= delta_y;
        protocol::MouseEvent mouse_event;
        mouse_event.set_wheel_delta_x(delta_x);
        mouse_event.set_wheel_delta_y(delta_y);

        // Always include the ticks in the event, even if insufficient pixel
        // scrolling has accumulated for a single tick. This informs hosts
        // that can't inject pixel-based scroll events that the client will
        // accumulate them into tick-based scrolling, which gives a better
        // overall experience than trying to do this host-side.
        int ticks_x = static_cast<int>(wheel_ticks_x_);
        int ticks_y = static_cast<int>(wheel_ticks_y_);
        wheel_ticks_x_ -= ticks_x;
        wheel_ticks_y_ -= ticks_y;
        mouse_event.set_wheel_ticks_x(ticks_x);
        mouse_event.set_wheel_ticks_y(ticks_y);

        if (input_stub_)
          input_stub_->InjectMouseEvent(mouse_event);
      }
      return true;
    }

    case PP_INPUTEVENT_TYPE_CHAR:
      // Consume but ignore character input events.
      return true;

    default: {
      VLOG(0) << "Unhandled input event: " << event.GetType();
      break;
    }
  }

  return false;
}

void PepperInputHandler::AllowMouseLock() {
  DCHECK_EQ(mouse_lock_state_, MouseLockDisallowed);
  mouse_lock_state_ = MouseLockOff;
}

void PepperInputHandler::DidChangeFocus(bool has_focus) {
  has_focus_ = has_focus;
  if (has_focus_)
    RequestMouseLock();
}

void PepperInputHandler::SetMouseCursor(scoped_ptr<pp::ImageData> image,
                                        const pp::Point& hotspot) {
  cursor_image_ = image.Pass();
  cursor_hotspot_ = hotspot;

  if (mouse_lock_state_ != MouseLockDisallowed && !cursor_image_) {
    RequestMouseLock();
  } else {
    CancelMouseLock();
  }
}

void PepperInputHandler::MouseLockLost() {
  DCHECK(mouse_lock_state_ == MouseLockOn ||
         mouse_lock_state_ == MouseLockCancelling);

  mouse_lock_state_ = MouseLockOff;
  UpdateMouseCursor();
}

void PepperInputHandler::RequestMouseLock() {
  // Request mouse lock only if the plugin is focused, the host-supplied cursor
  // is empty and no callback is pending.
  if (has_focus_ && !cursor_image_ && mouse_lock_state_ == MouseLockOff) {
    pp::CompletionCallback callback =
        callback_factory_.NewCallback(&PepperInputHandler::OnMouseLocked);
    int result = pp::MouseLock::LockMouse(callback);
    DCHECK_EQ(result, PP_OK_COMPLETIONPENDING);

    // Hide cursor to avoid it becoming a black square (see crbug.com/285809).
    pp::MouseCursor::SetCursor(instance_, PP_MOUSECURSOR_TYPE_NONE);

    mouse_lock_state_ = MouseLockRequestPending;
  }
}

void PepperInputHandler::CancelMouseLock() {
  switch (mouse_lock_state_) {
    case MouseLockDisallowed:
    case MouseLockOff:
      UpdateMouseCursor();
      break;

    case MouseLockCancelling:
      break;

    case MouseLockRequestPending:
      // The mouse lock request is pending. Delay UnlockMouse() call until
      // the callback is called.
      mouse_lock_state_ = MouseLockCancelling;
      break;

    case MouseLockOn:
      pp::MouseLock::UnlockMouse();

      // Note that mouse-lock has been cancelled. We will continue to receive
      // locked events until MouseLockLost() is called back.
      mouse_lock_state_ = MouseLockCancelling;
      break;

    default:
      NOTREACHED();
  }
}

void PepperInputHandler::UpdateMouseCursor() {
  DCHECK(mouse_lock_state_ == MouseLockDisallowed ||
         mouse_lock_state_ == MouseLockOff);

  if (cursor_image_) {
    pp::MouseCursor::SetCursor(instance_, PP_MOUSECURSOR_TYPE_CUSTOM,
                               *cursor_image_,
                               cursor_hotspot_);
  } else {
    // If there is no cursor shape stored, either because the host never
    // supplied one, or we were previously in mouse-lock mode, then use
    // a standard arrow pointer.
    pp::MouseCursor::SetCursor(instance_, PP_MOUSECURSOR_TYPE_POINTER);
  }
}

void PepperInputHandler::OnMouseLocked(int error) {
  DCHECK(mouse_lock_state_ == MouseLockRequestPending ||
         mouse_lock_state_ == MouseLockCancelling);

  bool should_cancel = (mouse_lock_state_ == MouseLockCancelling);

  // See if the operation succeeded.
  if (error == PP_OK) {
    mouse_lock_state_ = MouseLockOn;
  } else {
    mouse_lock_state_ = MouseLockOff;
    UpdateMouseCursor();
  }

  // Cancel as needed.
  if (should_cancel)
    CancelMouseLock();
}

}  // namespace remoting
