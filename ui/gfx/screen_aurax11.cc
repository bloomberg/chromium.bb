// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include <X11/Xlib.h>

#include "base/logging.h"
#include "ui/base/x/x11_util.h"

#if !defined(USE_ASH)

namespace gfx {
namespace {
gfx::Size Screen::GetPrimaryMonitorSize() {
  ::Display* display = ui::GetXDisplay();
  ::Screen* screen = DefaultScreenOfDisplay(display);
  int width = WidthOfScreen(screen);
  int height = HeightOfScreen(screen);

  return gfx::Size(width, height);
}
}  // namespace

// TODO(piman,erg): This file needs to be rewritten by someone who understands
// the subtlety of X11. That is not erg.

// static
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

// static
gfx::NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  // TODO(erg): I have no clue. May need collaboration with
  // RootWindowHostLinux?
  return NULL;
}

// static
int Screen::GetNumMonitors() {
  // TODO(erg): Figure this out with oshima or piman because I have no clue
  // about the Xinerama implications here.
  return 1;
}

// static
Monitor Screen::GetMonitorNearestWindow(NativeWindow window) {
  // TODO(erg): We need to eventually support multiple monitors.
  return GetPrimaryMonitor();
}

// static
Monitor Screen::GetMonitorNearestPoint(const Point& point) {
  // TODO(erg): We need to eventually support multiple monitors.
  return GetPrimaryMonitor();
}

// static
Monitor Screen::GetPrimaryMonitor() {
  // TODO(erg): There was a comment about how we shouldn't use _NET_WORKAREA
  // for work area by danakj@.
  // TODO(jamiewalch): Restrict work area to the actual work area of
  // the monitor.
  return Monitor(gfx::Rect(GetPrimaryMonitorSize()));
}

// static
Monitor Screen::GetMonitorMatching(const gfx::Rect& match_rect) {
  return GetPrimaryMonitor();
}

}  // namespace gfx
#endif  // !defined(USE_ASH)
