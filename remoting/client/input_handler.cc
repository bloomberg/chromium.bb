// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input_handler.h"

#include "remoting/client/chromoting_view.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/connection_to_host.h"

namespace remoting {

InputHandler::InputHandler(ClientContext* context,
                           protocol::ConnectionToHost* connection,
                           ChromotingView* view)
    : context_(context),
      connection_(connection),
      view_(view) {
}

void InputHandler::SendKeyEvent(bool press, int keycode) {
  KeyEvent* event = new KeyEvent();
  event->set_key(keycode);
  event->set_pressed(press);

  protocol::InputStub* stub = connection_->input_stub();
  stub->InjectKeyEvent(event, new DeleteTask<KeyEvent>(event));
}

void InputHandler::SendMouseMoveEvent(int x, int y) {
  MouseEvent* event = new MouseEvent();
  event->set_x(x);
  event->set_y(y);

  protocol::InputStub* stub = connection_->input_stub();
  stub->InjectMouseEvent(event, new DeleteTask<MouseEvent>(event));
}

void InputHandler::SendMouseButtonEvent(bool button_down,
                                        MouseButton button) {
  MouseEvent* event = new MouseEvent();
  event->set_button(button);
  event->set_button_down(button_down);

  protocol::InputStub* stub = connection_->input_stub();
  stub->InjectMouseEvent(event, new DeleteTask<MouseEvent>(event));
}

}  // namespace remoting
