// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_input_handler.h"

#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/point.h"
#include "remoting/client/plugin/pepper_view_proxy.h"

namespace remoting {

using protocol::MouseEvent;

PepperInputHandler::PepperInputHandler(ClientContext* context,
                                       protocol::ConnectionToHost* connection,
                                       PepperViewProxy* view)
    : InputHandler(context, connection, view),
      pepper_view_(view),
      wheel_ticks_x_(0),
      wheel_ticks_y_(0) {
}

PepperInputHandler::~PepperInputHandler() {
}

void PepperInputHandler::Initialize() {
}

void PepperInputHandler::HandleKeyEvent(bool keydown,
                                        const pp::KeyboardInputEvent& event) {
  SendKeyEvent(keydown, event.GetKeyCode());
}

void PepperInputHandler::HandleCharacterEvent(
    const pp::KeyboardInputEvent& event) {
  // TODO(garykac): Coordinate key and char events.
}

void PepperInputHandler::HandleMouseMoveEvent(
    const pp::MouseInputEvent& event) {
  SkIPoint p(SkIPoint::Make(event.GetPosition().x(), event.GetPosition().y()));
  // Pepper gives co-ordinates in the plugin instance's co-ordinate system,
  // which may be different from the host desktop's co-ordinate system.
  double horizontal_ratio = view_->GetHorizontalScaleRatio();
  double vertical_ratio = view_->GetVerticalScaleRatio();

  if (horizontal_ratio == 0.0)
    horizontal_ratio = 1.0;
  if (vertical_ratio == 0.0)
    vertical_ratio = 1.0;

  SendMouseMoveEvent(p.x() / horizontal_ratio, p.y() / vertical_ratio);
}

void PepperInputHandler::HandleMouseButtonEvent(
    bool button_down,
    const pp::MouseInputEvent& event) {
  MouseEvent::MouseButton button = MouseEvent::BUTTON_UNDEFINED;
  switch (event.GetButton()) {
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      button = MouseEvent::BUTTON_LEFT;
      break;
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      button = MouseEvent::BUTTON_MIDDLE;
      break;
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      button = MouseEvent::BUTTON_RIGHT;
      break;
    case PP_INPUTEVENT_MOUSEBUTTON_NONE:
      // Leave button undefined.
      break;
  }

  if (button != MouseEvent::BUTTON_UNDEFINED) {
    SendMouseButtonEvent(button_down, button);
  }
}

void PepperInputHandler::HandleMouseWheelEvent(
    const pp::WheelInputEvent& event) {
  pp::FloatPoint ticks = event.GetTicks();
  wheel_ticks_x_ += ticks.x();
  wheel_ticks_y_ += ticks.y();
  int ticks_x = static_cast<int>(wheel_ticks_x_);
  int ticks_y = static_cast<int>(wheel_ticks_y_);
  if (ticks_x != 0 || ticks_y != 0) {
    wheel_ticks_x_ -= ticks_x;
    wheel_ticks_y_ -= ticks_y;
    SendMouseWheelEvent(ticks_x, ticks_y);
  }
}

}  // namespace remoting
