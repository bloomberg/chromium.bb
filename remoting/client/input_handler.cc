// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input_handler.h"

#include "remoting/client/chromoting_view.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/input_stub.h"

namespace remoting {

using protocol::KeyEvent;
using protocol::MouseEvent;

InputHandler::InputHandler(ClientContext* context,
                           protocol::ConnectionToHost* connection,
                           ChromotingView* view)
    : context_(context),
      connection_(connection),
      view_(view) {
}

InputHandler::~InputHandler() {
}

void InputHandler::SendKeyEvent(bool press, int keycode) {
  protocol::InputStub* stub = connection_->input_stub();
  if (stub) {
    if (press) {
      pressed_keys_.insert(keycode);
    } else {
      pressed_keys_.erase(keycode);
    }

    KeyEvent event;
    event.set_keycode(keycode);
    event.set_pressed(press);
    stub->InjectKeyEvent(event);
  }
}

void InputHandler::SendMouseMoveEvent(int x, int y) {
  protocol::InputStub* stub = connection_->input_stub();
  if (stub) {
    MouseEvent event;
    event.set_x(x);
    event.set_y(y);
    stub->InjectMouseEvent(event);
  }
}

void InputHandler::SendMouseButtonEvent(bool button_down,
                                        MouseEvent::MouseButton button) {
  protocol::InputStub* stub = connection_->input_stub();
  if (stub) {
    MouseEvent event;
    event.set_button(button);
    event.set_button_down(button_down);
    stub->InjectMouseEvent(event);
  }
}

void InputHandler::ReleaseAllKeys() {
  std::set<int> pressed_keys_copy = pressed_keys_;
  std::set<int>::iterator i;
  for (i = pressed_keys_copy.begin(); i != pressed_keys_copy.end(); ++i) {
    SendKeyEvent(false, *i);
  }
  pressed_keys_.clear();
}

}  // namespace remoting
