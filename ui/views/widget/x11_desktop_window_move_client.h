// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_
#define UI_VIEWS_WIDGET_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "ui/aura/client/window_move_client.h"
#include "ui/aura/event_filter.h"
#include "ui/views/views_export.h"
#include "ui/gfx/point.h"

namespace views {

// When we're dragging tabs, we need to manually position our window.
class VIEWS_EXPORT X11DesktopWindowMoveClient
    : public aura::EventFilter,
      public aura::client::WindowMoveClient {
 public:
  X11DesktopWindowMoveClient();
  virtual ~X11DesktopWindowMoveClient();

  // Overridden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

  // Overridden from aura::client::WindowMoveClient:
  virtual aura::client::WindowMoveResult RunMoveLoop(
      aura::Window* window,
      const gfx::Point& drag_offset) OVERRIDE;
  virtual void EndMoveLoop() OVERRIDE;

 private:
  // Are we running a nested message loop from RunMoveLoop()?
  bool in_move_loop_;

  // Our cursor offset from the top left window origin when the drag
  // started. Used to calculate the window's new bounds relative to the current
  // location of the cursor.
  gfx::Point window_offset_;

  base::Closure quit_closure_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_X11_DESKTOP_WINDOW_MOVE_CLIENT_H_
