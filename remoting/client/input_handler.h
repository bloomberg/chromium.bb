// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_HANDLER_H_
#define REMOTING_CLIENT_INPUT_HANDLER_H_

#include "base/basictypes.h"
#include "base/task.h"
#include "remoting/proto/event.pb.h"

namespace remoting {

class ClientContext;
class ChromotingView;

namespace protocol {
class ConnectionToHost;
}  // namespace protocol

class InputHandler {
 public:
  InputHandler(ClientContext* context,
               protocol::ConnectionToHost* connection,
               ChromotingView* view);
  virtual ~InputHandler() {}

  virtual void Initialize() = 0;

 protected:
  void SendKeyEvent(bool press, int keycode);
  void SendMouseMoveEvent(int x, int y);
  void SendMouseButtonEvent(bool down, MouseButton button);

  ClientContext* context_;
  protocol::ConnectionToHost* connection_;
  ChromotingView* view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputHandler);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::InputHandler);

#endif  // REMOTING_CLIENT_INPUT_HANDLER_H_
