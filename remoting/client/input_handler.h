// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_HANDLER_H_
#define REMOTING_CLIENT_INPUT_HANDLER_H_

#include "base/basictypes.h"
#include "base/task.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

class ClientContext;
class ChromotingView;
class HostConnection;

class InputHandler {
 public:
  InputHandler(ClientContext* context,
               HostConnection* connection,
               ChromotingView* view);
  virtual ~InputHandler() {}

  virtual void Initialize() = 0;

 protected:
  void SendKeyEvent(bool press, int keycode);
  void SendMouseMoveEvent(int x, int y);
  void SendMouseButtonEvent(bool down, MouseButton button);

  ClientContext* context_;
  HostConnection* connection_;
  ChromotingView* view_;

 private:
  // True if we should send the next mouse position as an absolute value rather
  // than a relative value. After sending a single absolute mouse position,
  // it will automatically switch back to sending relative mouse deltas.
  bool send_absolute_mouse_;

  // Current (x,y) position of mouse pointer.
  // This is the last value that we sent to the host.
  int mouse_x_, mouse_y_;

  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::InputHandler);

#endif  // REMOTING_CLIENT_INPUT_HANDLER_H_
