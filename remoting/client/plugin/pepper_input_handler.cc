// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_input_handler.h"

#include "third_party/ppapi/c/pp_input_event.h"

namespace remoting {

PepperInputHandler::PepperInputHandler(ClientContext* context,
                                       HostConnection* connection,
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

void PepperInputHandler::HandleMouseMoveEvent(const PP_InputEvent_Mouse& event) {
  SendMouseMoveEvent(static_cast<int>(event.x),
                     static_cast<int>(event.y));
}

void PepperInputHandler::HandleMouseButtonEvent(
    bool button_down,
    const PP_InputEvent_Mouse& event) {
  MouseButton button = MouseButtonUndefined;
  if (event.button == PP_INPUTEVENT_MOUSEBUTTON_LEFT) {
    button = MouseButtonLeft;
  } else if (event.button == PP_INPUTEVENT_MOUSEBUTTON_MIDDLE) {
    button = MouseButtonMiddle;
  } else if (event.button == PP_INPUTEVENT_MOUSEBUTTON_RIGHT) {
    button = MouseButtonRight;
  }

  if (button != MouseButtonUndefined) {
    SendMouseButtonEvent(button_down, button);
  }
}

}  // namespace remoting
