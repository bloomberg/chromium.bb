// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_X11_INPUT_HANDLER_H_
#define REMOTING_CLIENT_X11_INPUT_HANDLER_H_

#include "remoting/client/input_handler.h"

namespace remoting {

class ClientContext;
class ChromotingView;

class X11InputHandler : public InputHandler {
 public:
  X11InputHandler(ClientContext* context, ChromotingView* view);
  virtual ~X11InputHandler();

  void Initialize();

 private:

  void DoProcessX11Events();

  void ScheduleX11EventHandler();

  ClientContext* context_;
  ChromotingView* view_;

  DISALLOW_COPY_AND_ASSIGN(X11InputHandler);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::X11InputHandler);

#endif  // REMOTING_CLIENT_X11_INPUT_HANDLER_H_
