// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/x11_input_handler.h"

#include "base/message_loop.h"
#include "remoting/client/client_context.h"
#include "remoting/client/x11_view.h"
#include "remoting/jingle_glue/jingle_thread.h"

// Include Xlib at the end because it clashes with ClientMessage defined in
// the protocol buffer.
#include <X11/Xlib.h>

namespace remoting {

X11InputHandler::X11InputHandler(ClientContext* context,
                                 ChromotingView* view)
    : context_(context),
      view_(view) {
}

X11InputHandler::~X11InputHandler() {
}

void X11InputHandler::Initialize() {
  ScheduleX11EventHandler();
}

void X11InputHandler::DoProcessX11Events() {
  DCHECK_EQ(context_->jingle_thread()->message_loop(), MessageLoop::current());
  Display* display = static_cast<X11View*>(view_)->display();
  if (XPending(display)) {
    XEvent e;
    XNextEvent(display, &e);
    if (e.type == Expose) {
      // Tell the view to paint again.
      view_->Paint();
    } else if (e.type == ButtonPress) {
      // TODO(hclam): Implement.
      NOTIMPLEMENTED();
    } else {
      // TODO(hclam): Implement.
      NOTIMPLEMENTED();
    }
  }

  // Schedule the next event handler.
  ScheduleX11EventHandler();
}

void X11InputHandler::ScheduleX11EventHandler() {
  // Schedule a delayed task to process X11 events in 10ms.
  static const int kProcessEventsInterval = 10;
  context_->jingle_thread()->message_loop()->PostDelayedTask(
      FROM_HERE,
      NewRunnableMethod(this, &X11InputHandler::DoProcessX11Events),
      kProcessEventsInterval);
}

}  // namespace remoting
