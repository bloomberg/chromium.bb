// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/input_handler.h"

#include "remoting/client/chromoting_view.h"
#include "remoting/client/host_connection.h"

namespace remoting {

InputHandler::InputHandler(ClientContext* context,
                           HostConnection* connection,
                           ChromotingView* view)
    : context_(context),
      connection_(connection),
      view_(view),
      send_absolute_mouse_(true),
      mouse_x_(0),
      mouse_y_(0) {
}

void InputHandler::SendKeyEvent(bool press, int keycode) {
  ChromotingClientMessage msg;

  KeyEvent *event = msg.mutable_key_event();
  event->set_key(keycode);
  event->set_pressed(press);

  connection_->SendEvent(msg);
}

void InputHandler::SendMouseMoveEvent(int x, int y) {
  ChromotingClientMessage msg;

  if (send_absolute_mouse_) {
    MouseSetPositionEvent *event = msg.mutable_mouse_set_position_event();
    event->set_x(x);
    event->set_y(y);

    int width, height;
    view_->GetScreenSize(&width, &height);
    event->set_width(width);
    event->set_height(height);

    // TODO(garykac): Fix drift problem with relative mouse events and
    // then re-add this line.
    //send_absolute_mouse_ = false;
  } else {
    MouseMoveEvent *event = msg.mutable_mouse_move_event();
    int dx = x - mouse_x_;
    int dy = y - mouse_y_;
    if (dx == 0 && dy == 0) {
      return;
    }
    event->set_offset_x(dx);
    event->set_offset_y(dy);
  }
  // Record current mouse position.
  mouse_x_ = x;
  mouse_y_ = y;

  connection_->SendEvent(msg);
}

void InputHandler::SendMouseButtonEvent(bool button_down,
                                        MouseButton button) {
  ChromotingClientMessage msg;

  if (button_down) {
    msg.mutable_mouse_down_event()->set_button(button);
  } else {
    msg.mutable_mouse_up_event()->set_button(button);
  }

  connection_->SendEvent(msg);
}

}  // namespace remoting
