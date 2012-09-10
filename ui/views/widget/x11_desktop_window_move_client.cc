// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/x11_desktop_window_move_client.h"

#include <X11/Xlib.h>
// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/message_loop.h"
#include "base/message_pump_aurax11.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/screen.h"

namespace views {

X11DesktopWindowMoveClient::X11DesktopWindowMoveClient()
    : in_move_loop_(false) {
}

X11DesktopWindowMoveClient::~X11DesktopWindowMoveClient() {}

bool X11DesktopWindowMoveClient::PreHandleKeyEvent(aura::Window* target,
                                                   ui::KeyEvent* event) {
  return false;
}

bool X11DesktopWindowMoveClient::PreHandleMouseEvent(aura::Window* target,
                                                     ui::MouseEvent* event) {
  if (in_move_loop_) {
    switch (event->type()) {
      case ui::ET_MOUSE_DRAGGED:
      case ui::ET_MOUSE_MOVED: {
        DCHECK(event->valid_system_location());
        gfx::Point system_loc =
            event->system_location().Subtract(window_offset_);
        aura::RootWindow* root_window = target->GetRootWindow();
        root_window->SetHostBounds(gfx::Rect(
            system_loc, root_window->GetHostSize()));
        return true;
      }
      case ui::ET_MOUSE_CAPTURE_CHANGED:
      case ui::ET_MOUSE_RELEASED: {
        EndMoveLoop();
        return true;
      }
      default:
        break;
    }
  }

  return false;
}

ui::TouchStatus X11DesktopWindowMoveClient::PreHandleTouchEvent(
    aura::Window* target,
    ui::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::EventResult X11DesktopWindowMoveClient::PreHandleGestureEvent(
    aura::Window* target,
    ui::GestureEvent* event) {
  return ui::ER_UNHANDLED;
}

aura::client::WindowMoveResult X11DesktopWindowMoveClient::RunMoveLoop(
    aura::Window* source,
    const gfx::Point& drag_offset) {
  DCHECK(!in_move_loop_);  // Can only handle one nested loop at a time.
  in_move_loop_ = true;
  window_offset_ = drag_offset;

  source->GetRootWindow()->ShowRootWindow();

  Display* display = base::MessagePumpAuraX11::GetDefaultXDisplay();
  XGrabServer(display);
  XUngrabPointer(display, CurrentTime);

  aura::RootWindow* root_window = source->GetRootWindow();
  int ret = XGrabPointer(display,
                         root_window->GetAcceleratedWidget(),
                         False,
                         ButtonReleaseMask | PointerMotionMask,
                         GrabModeAsync,
                         GrabModeAsync,
                         None,
                         None,
                         CurrentTime);
  XUngrabServer(display);
  if (ret != GrabSuccess) {
    DLOG(ERROR) << "Grabbing new tab for dragging failed: " << ret;
    return aura::client::MOVE_CANCELED;
  }

  MessageLoopForUI* loop = MessageLoopForUI::current();
  MessageLoop::ScopedNestableTaskAllower allow_nested(loop);
  base::RunLoop run_loop(aura::Env::GetInstance()->GetDispatcher());
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return aura::client::MOVE_SUCCESSFUL;
}

void X11DesktopWindowMoveClient::EndMoveLoop() {
  if (!in_move_loop_)
    return;

  // TODO(erg): Is this ungrab the cause of having to click to give input focus
  // on drawn out windows? Not ungrabbing here screws the X server until I kill
  // the chrome process.

  // Ungrab before we let go of the window.
  Display* display = base::MessagePumpAuraX11::GetDefaultXDisplay();
  XUngrabPointer(display, CurrentTime);

  in_move_loop_ = false;
  quit_closure_.Run();
}

}  // namespace views
