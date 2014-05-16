// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"

#include "ui/aura/window.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_x11.h"

namespace views {

X11TopmostWindowFinder::X11TopmostWindowFinder() : toplevel_(None) {
}

X11TopmostWindowFinder::~X11TopmostWindowFinder() {
}

aura::Window* X11TopmostWindowFinder::FindLocalProcessWindowAt(
    const gfx::Point& screen_loc,
    const std::set<aura::Window*>& ignore) {
  screen_loc_ = screen_loc;
  ignore_ = ignore;

  std::vector<aura::Window*> local_process_windows =
      DesktopWindowTreeHostX11::GetAllOpenWindows();
  bool found_local_process_window = false;
  for (size_t i = 0; i < local_process_windows.size(); ++i) {
    if (ShouldStopIteratingAtLocalProcessWindow(local_process_windows[i])) {
      found_local_process_window = true;
      break;
    }
  }
  if (!found_local_process_window)
    return NULL;

  ui::EnumerateTopLevelWindows(this);
  return views::DesktopWindowTreeHostX11::GetContentWindowForXID(toplevel_);
}

XID X11TopmostWindowFinder::FindWindowAt(const gfx::Point& screen_loc) {
  screen_loc_ = screen_loc;
  ui::EnumerateTopLevelWindows(this);
  return toplevel_;
}

bool X11TopmostWindowFinder::ShouldStopIterating(XID xid) {
  if (!ui::IsWindowVisible(xid))
    return false;

  aura::Window* window =
      views::DesktopWindowTreeHostX11::GetContentWindowForXID(xid);
  if (window) {
    if (ShouldStopIteratingAtLocalProcessWindow(window)) {
      toplevel_ = xid;
      return true;
    }
    return false;
  }

  if (ui::WindowContainsPoint(xid, screen_loc_)) {
    toplevel_ = xid;
    return true;
  }
  return false;
}

bool X11TopmostWindowFinder::ShouldStopIteratingAtLocalProcessWindow(
    aura::Window* window) {
  if (ignore_.find(window) != ignore_.end())
    return false;

  // Currently |window|->IsVisible() always returns true.
  // TODO(pkotwicz): Fix this. crbug.com/353038
  return window->IsVisible() &&
      window->GetBoundsInScreen().Contains(screen_loc_);
}

}  // namespace views
