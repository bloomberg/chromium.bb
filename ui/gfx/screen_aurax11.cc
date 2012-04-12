// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include <X11/Xlib.h>

#include "base/logging.h"
#include "ui/base/x/x11_util.h"

#if !defined(USE_ASH)

namespace gfx {

// TODO(piman,erg): This file needs to be rewritten by someone who understands
// the subtlety of X11. That is not erg.

gfx::Point Screen::GetCursorScreenPoint() {
  Display* display = ui::GetXDisplay();

  // Unsure if I can leave these as NULL.
  ::Window root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;
  XQueryPointer(display,
                DefaultRootWindow(display),
                &root,
                &child,
                &root_x,
                &root_y,
                &win_x,
                &win_y,
                &mask);

  return gfx::Point(root_x, root_y);
}

gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(
      gfx::NativeView view) {
  // TODO(erg): There was a comment about how we shouldn't use _NET_WORKAREA
  // here by danakj@.
  return GetMonitorAreaNearestWindow(view);
}

gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeView view) {
  // TODO(erg): Yet another stub.
  return GetPrimaryMonitorBounds();
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestPoint(const gfx::Point& point) {
  // TODO(jamiewalch): Restrict this to the work area of the monitor.
  return GetMonitorAreaNearestPoint(point);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestPoint(const gfx::Point& point) {
  // TODO(erg): gdk actually has a description for this! We can implement this
  // one.
  return GetPrimaryMonitorBounds();
}

gfx::Rect Screen::GetPrimaryMonitorWorkArea() {
  // TODO(erg): Also needs a real implementation.
  return gfx::Rect(gfx::Point(0, 0), GetPrimaryMonitorSize());
}

gfx::Rect Screen::GetPrimaryMonitorBounds() {
  // TODO(erg): Probably needs to be smarter?
  return gfx::Rect(gfx::Point(0, 0), GetPrimaryMonitorSize());
}

gfx::Rect Screen::GetMonitorWorkAreaMatching(const gfx::Rect& match_rect) {
  // TODO(erg): We need to eventually support multiple monitors.
  return GetPrimaryMonitorWorkArea();
}

gfx::NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  // TODO(erg): I have no clue. May need collaboration with
  // RootWindowHostLinux?
  return NULL;
}

gfx::Size Screen::GetPrimaryMonitorSize() {
  ::Display* display = ui::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);

  return gfx::Size(width, height);
}

int Screen::GetNumMonitors() {
  // TODO(erg): Figure this out with oshima or piman because I have no clue
  // about the Xinerama implications here.
  return 1;
}

}  // namespace gfx

#endif  // !defined(USE_ASH)
