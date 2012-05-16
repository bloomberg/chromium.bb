// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_X11_WINDOW_EVENT_FILTER_H_
#define UI_VIEWS_WIDGET_X11_WINDOW_EVENT_FILTER_H_
#pragma once

#include <X11/Xlib.h>
// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/compiler_specific.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ui/views/views_export.h"

namespace aura {
class RootWindow;
}

namespace views {

// An EventFilter that sets properties on X11 windows.
class VIEWS_EXPORT X11WindowEventFilter : public aura::EventFilter {
 public:
  explicit X11WindowEventFilter(aura::RootWindow* root_window);
  virtual ~X11WindowEventFilter();

  // Changes whether borders are shown on this |root_window|.
  void SetUseHostWindowBorders(bool use_os_border);

  // Overridden from EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  // Dispatches a _NET_WM_MOVERESIZE message to the window manager to tell it
  // to act as if a border or titlebar drag occurred.
  bool DispatchHostWindowDragMovement(int hittest,
                                      const gfx::Point& screen_location);

  aura::RootWindow* root_window_;

  // The display and the native X window hosting the root window.
  Display* xdisplay_;
  ::Window xwindow_;

  // The native root window.
  ::Window x_root_window_;

  DISALLOW_COPY_AND_ASSIGN(X11WindowEventFilter);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_X11_WINDOW_EVENT_FILTER_H_
