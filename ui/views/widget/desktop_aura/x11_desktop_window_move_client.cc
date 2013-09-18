// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client.h"

#include <X11/Xlib.h>
// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/debug/stack_trace.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_x11.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"

namespace views {

X11DesktopWindowMoveClient::X11DesktopWindowMoveClient()
    : move_loop_(this),
      root_window_(NULL) {
}

X11DesktopWindowMoveClient::~X11DesktopWindowMoveClient() {}

void X11DesktopWindowMoveClient::OnMouseMovement(XMotionEvent* event) {
  gfx::Point cursor_point(event->x_root, event->y_root);
  gfx::Point system_loc = cursor_point - window_offset_;
  root_window_->SetHostBounds(gfx::Rect(
      system_loc, root_window_->GetHostSize()));
}

void X11DesktopWindowMoveClient::OnMouseReleased() {
  EndMoveLoop();
}

void X11DesktopWindowMoveClient::OnMoveLoopEnded() {
  root_window_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, aura::client::WindowMoveClient implementation:

aura::client::WindowMoveResult X11DesktopWindowMoveClient::RunMoveLoop(
    aura::Window* source,
    const gfx::Vector2d& drag_offset,
    aura::client::WindowMoveSource move_source) {
  window_offset_ = drag_offset;
  root_window_ = source->GetRootWindow();

  bool success = move_loop_.RunMoveLoop(source, root_window_->last_cursor());
  return success ? aura::client::MOVE_SUCCESSFUL : aura::client::MOVE_CANCELED;
}

void X11DesktopWindowMoveClient::EndMoveLoop() {
  move_loop_.EndMoveLoop();
}

}  // namespace views
