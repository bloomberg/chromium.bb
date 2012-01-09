// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_input_handler.h"

#include "base/logging.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/point.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

PepperInputHandler::PepperInputHandler(protocol::InputStub* input_stub)
    : input_stub_(input_stub), wheel_ticks_x_(0), wheel_ticks_y_(0)
{
}

PepperInputHandler::~PepperInputHandler() {
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
      protocol::KeyEvent key_event;
      key_event.set_keycode(pp_key_event.GetKeyCode());
      key_event.set_pressed(event.GetType() == PP_INPUTEVENT_TYPE_KEYDOWN);
      input_stub_->InjectKeyEvent(key_event);
      return true;
    }

    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
    case PP_INPUTEVENT_TYPE_MOUSEUP: {
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
        input_stub_->InjectMouseEvent(mouse_event);
      }
      return true;
    }

    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE: {
      pp::MouseInputEvent pp_mouse_event(event);
      protocol::MouseEvent mouse_event;
      mouse_event.set_x(pp_mouse_event.GetPosition().x());
      mouse_event.set_y(pp_mouse_event.GetPosition().y());
      input_stub_->InjectMouseEvent(mouse_event);
      return true;
    }

    case PP_INPUTEVENT_TYPE_WHEEL: {
      pp::WheelInputEvent pp_wheel_event(event);

      pp::FloatPoint ticks = pp_wheel_event.GetTicks();
      wheel_ticks_x_ += ticks.x();
      wheel_ticks_y_ += ticks.y();

      int ticks_x = static_cast<int>(wheel_ticks_x_);
      int ticks_y = static_cast<int>(wheel_ticks_y_);
      if (ticks_x != 0 || ticks_y != 0) {
        wheel_ticks_x_ -= ticks_x;
        wheel_ticks_y_ -= ticks_y;
        protocol::MouseEvent mouse_event;
        mouse_event.set_wheel_offset_x(ticks_x);
        mouse_event.set_wheel_offset_y(ticks_y);
        input_stub_->InjectMouseEvent(mouse_event);
      }
      return true;
    }

    default: {
      LOG(INFO) << "Unhandled input event: " << event.GetType();
      break;
    }
  }

  return false;
}

}  // namespace remoting
