// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"
#include "ui/gfx/monitor.h"

namespace gfx {

// static
gfx::Monitor Screen::GetPrimaryMonitor() {
  NOTIMPLEMENTED() << "crbug.com/117839 tracks implementation";
  return gfx::Monitor(0, gfx::Rect(0, 0, 1, 1));
}

// static
gfx::Monitor Screen::GetMonitorNearestWindow(gfx::NativeView view) {
  return GetPrimaryMonitor();
}

// static
int Screen::GetNumMonitors() {
  return 1;
}

}  // namespace gfx
