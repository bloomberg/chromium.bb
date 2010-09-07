// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/x11_input_handler.h"

#include "base/message_loop.h"
#include "remoting/client/client_context.h"
#include "remoting/client/x11_view.h"
#include "remoting/jingle_glue/jingle_thread.h"

// Include Xlib at the end because it clashes with Status in
// base/tracked_objects.h.
#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace remoting {

X11InputHandler::X11InputHandler(ClientContext* context,
                                 HostConnection* connection,
                                 ChromotingView* view)
    : InputHandler(context, connection, view) {
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
    switch (e.type) {
      case Expose:
        // Tell the view to paint again.
        view_->Paint();
        break;
      case KeyPress:
      case KeyRelease:
        HandleKeyEvent(&e);
        break;
      case ButtonPress:
        HandleMouseButtonEvent(true, e.xbutton.button);
        HandleMouseMoveEvent(e.xbutton.x, e.xbutton.y);
        break;
      case ButtonRelease:
        HandleMouseButtonEvent(false, e.xbutton.button);
        HandleMouseMoveEvent(e.xbutton.x, e.xbutton.y);
        break;
      case MotionNotify:
        SendMouseMoveEvent(e.xmotion.x, e.xmotion.y);
        break;
      default:
        LOG(WARNING) << "Unknown event type: " << e.type;
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

void X11InputHandler::HandleKeyEvent(void* event) {
  XEvent* e = reinterpret_cast<XEvent*>(event);
  char buffer[128];
  int buffsize = sizeof(buffer) - 1;
  KeySym keysym;
  XLookupString(&e->xkey, buffer, buffsize, &keysym, NULL);
  SendKeyEvent(e->type == KeyPress, static_cast<int>(keysym));
}

void X11InputHandler::HandleMouseMoveEvent(int x, int y) {
  SendMouseMoveEvent(x, y);
}

void X11InputHandler::HandleMouseButtonEvent(bool button_down, int xbutton_id) {
  MouseButton button = MouseButtonUndefined;
  if (xbutton_id == 1) {
    button = MouseButtonLeft;
  } else if (xbutton_id == 2) {
    button = MouseButtonMiddle;
  } else if (xbutton_id == 3) {
    button = MouseButtonRight;
  }

  if (button != MouseButtonUndefined) {
    SendMouseButtonEvent(button_down, button);
  }
}

}  // namespace remoting
