// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include "base/logging.h"
#include "ui/gfx/display.h"

namespace gfx {

// static
bool Screen::IsDIPEnabled() {
  return false;
}

// static
gfx::Display Screen::GetPrimaryDisplay() {
  NOTIMPLEMENTED() << "crbug.com/117839 tracks implementation";
  return gfx::Display(0, gfx::Rect(0, 0, 1, 1));
}

// static
gfx::Display Screen::GetDisplayNearestWindow(gfx::NativeView view) {
  return GetPrimaryDisplay();
}

// static
gfx::Display Screen::GetDisplayNearestPoint(const gfx::Point& point) {
  return GetPrimaryDisplay();
}

int Screen::GetNumDisplays() {
  return 1;
}

}  // namespace gfx
