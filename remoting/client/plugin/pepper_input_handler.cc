// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_input_handler.h"

#include "ppapi/c/pp_input_event.h"
#include "remoting/client/chromoting_view.h"
#include "ui/gfx/point.h"

namespace remoting {

using protocol::KeyEvent;
using protocol::MouseEvent;

PepperInputHandler::PepperInputHandler(ClientContext* context,
                                       protocol::ConnectionToHost* connection,
                                       ChromotingView* view)
  : InputHandler(context, connection, view) {
}

PepperInputHandler::~PepperInputHandler() {
}

void PepperInputHandler::Initialize() {
}

void PepperInputHandler::HandleKeyEvent(bool keydown,
                                        const PP_InputEvent_Key& event) {
  SendKeyEvent(keydown, event.key_code);
}

void PepperInputHandler::HandleCharacterEvent(
    const PP_InputEvent_Character& event) {
  // TODO(garykac): Coordinate key and char events.
}

void PepperInputHandler::HandleMouseMoveEvent(
    const PP_InputEvent_Mouse& event) {
  gfx::Point p(static_cast<int>(event.x), static_cast<int>(event.y));
  // Pepper gives co-ordinates in the plugin instance's co-ordinate system,
  // which may be different from the host desktop's co-ordinate system.
  p = view_->ConvertScreenToHost(p);
  SendMouseMoveEvent(p.x(), p.y());
}

void PepperInputHandler::HandleMouseButtonEvent(
    bool button_down,
    const PP_InputEvent_Mouse& event) {
  MouseEvent::MouseButton button = MouseEvent::BUTTON_UNDEFINED;
  if (event.button == PP_INPUTEVENT_MOUSEBUTTON_LEFT) {
    button = MouseEvent::BUTTON_LEFT;
  } else if (event.button == PP_INPUTEVENT_MOUSEBUTTON_MIDDLE) {
    button = MouseEvent::BUTTON_MIDDLE;
  } else if (event.button == PP_INPUTEVENT_MOUSEBUTTON_RIGHT) {
    button = MouseEvent::BUTTON_RIGHT;
  }

  if (button != MouseEvent::BUTTON_UNDEFINED) {
    SendMouseButtonEvent(button_down, button);
  }
}

}  // namespace remoting
