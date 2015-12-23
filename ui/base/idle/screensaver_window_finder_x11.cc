// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/screensaver_window_finder_x11.h"

#include "ui/base/x/x11_util.h"
#include "ui/gfx/x/x11_error_tracker.h"

namespace ui {

ScreensaverWindowFinder::ScreensaverWindowFinder()
    : exists_(false) {
}

bool ScreensaverWindowFinder::ScreensaverWindowExists() {
  gfx::X11ErrorTracker err_tracker;
  ScreensaverWindowFinder finder;
  ui::EnumerateTopLevelWindows(&finder);
  return finder.exists_ && !err_tracker.FoundNewError();
}

bool ScreensaverWindowFinder::ShouldStopIterating(XID window) {
  if (!ui::IsWindowVisible(window) || !IsScreensaverWindow(window))
    return false;
  exists_ = true;
  return true;
}

bool ScreensaverWindowFinder::IsScreensaverWindow(XID window) const {
  // It should occupy the full screen.
  if (!ui::IsX11WindowFullScreen(window))
    return false;

  // For xscreensaver, the window should have _SCREENSAVER_VERSION property.
  if (ui::PropertyExists(window, "_SCREENSAVER_VERSION"))
    return true;

  // For all others, like gnome-screensaver, the window's WM_CLASS property
  // should contain "screensaver".
  std::string value;
  if (!ui::GetStringProperty(window, "WM_CLASS", &value))
    return false;

  return value.find("screensaver") != std::string::npos;
}

}  // namespace ui
