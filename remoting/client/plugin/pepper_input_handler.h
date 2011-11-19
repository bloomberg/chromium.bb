// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_

#include "remoting/client/input_handler.h"

namespace pp {
class KeyboardInputEvent;
class MouseInputEvent;
class WheelInputEvent;
}

namespace remoting {

class PepperViewProxy;

class PepperInputHandler : public InputHandler {
 public:
  PepperInputHandler(ClientContext* context,
                     protocol::ConnectionToHost* connection,
                     PepperViewProxy* view);
  virtual ~PepperInputHandler();

  virtual void Initialize() OVERRIDE;

  void HandleKeyEvent(bool keydown, const pp::KeyboardInputEvent& event);
  void HandleCharacterEvent(const pp::KeyboardInputEvent& event);

  void HandleMouseMoveEvent(const pp::MouseInputEvent& event);
  void HandleMouseButtonEvent(bool button_down,
                              const pp::MouseInputEvent& event);
  void HandleMouseWheelEvent(const pp::WheelInputEvent& event);

 private:
  PepperViewProxy* pepper_view_;

  float wheel_ticks_x_;
  float wheel_ticks_y_;

  DISALLOW_COPY_AND_ASSIGN(PepperInputHandler);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_INPUT_HANDLER_H_
