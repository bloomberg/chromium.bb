// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_desktop_window_move_client.h"

#include <X11/Xlib.h>

#include "base/debug/stack_trace.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_x11.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/gfx/screen.h"

namespace {

// Delay moving the window.
//
// When we receive a mouse move event, we have to have it processed in a
// different run through the message pump because moving the window will
// otherwise prevent tasks from running.
//
// This constant was derived with playing with builds of chrome; it has no
// theoretical justification.
//
// TODO(erg): This helps with the performance of dragging windows, but it
// doesn't really solve the hard problems, which is that various calls to X11,
// such as XQueryPointer, ui::IsWindowVisible() and ui::WindowContainsPoint()
// take a while to get replies and block in the process. I've seen all of the
// above take as long as 20ms to respond.
const int kMoveDelay = 3;

}  // namespace

namespace views {

X11DesktopWindowMoveClient::X11DesktopWindowMoveClient()
    : move_loop_(this),
      dispatcher_(NULL) {
}

X11DesktopWindowMoveClient::~X11DesktopWindowMoveClient() {}

void X11DesktopWindowMoveClient::OnMouseMovement(XMotionEvent* event) {
  gfx::Point cursor_point(event->x_root, event->y_root);
  gfx::Point system_loc = cursor_point - window_offset_;

  gfx::Rect target_rect(system_loc, dispatcher_->host()->GetBounds().size());

  window_move_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kMoveDelay),
      base::Bind(&X11DesktopWindowMoveClient::SetHostBounds,
                 base::Unretained(this),
                 target_rect));
}

void X11DesktopWindowMoveClient::OnMouseReleased() {
  EndMoveLoop();
}

void X11DesktopWindowMoveClient::OnMoveLoopEnded() {
  dispatcher_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, aura::client::WindowMoveClient implementation:

aura::client::WindowMoveResult X11DesktopWindowMoveClient::RunMoveLoop(
    aura::Window* source,
    const gfx::Vector2d& drag_offset,
    aura::client::WindowMoveSource move_source) {
  window_offset_ = drag_offset;
  dispatcher_ = source->GetHost()->dispatcher();

  bool success = move_loop_.RunMoveLoop(source,
                                        dispatcher_->host()->last_cursor());
  return success ? aura::client::MOVE_SUCCESSFUL : aura::client::MOVE_CANCELED;
}

void X11DesktopWindowMoveClient::EndMoveLoop() {
  window_move_timer_.Stop();
  move_loop_.EndMoveLoop();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopWindowTreeHostLinux, private:

void X11DesktopWindowMoveClient::SetHostBounds(const gfx::Rect& rect) {
  dispatcher_->host()->SetBounds(rect);
}

}  // namespace views
