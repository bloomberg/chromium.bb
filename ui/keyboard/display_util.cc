// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/display_util.h"

#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace keyboard {

DisplayUtil::DisplayUtil() {}

int64_t DisplayUtil::GetNearestDisplayIdToWindow(aura::Window* window) const {
  return GetNearestDisplayToWindow(window).id();
}

display::Display DisplayUtil::GetNearestDisplayToWindow(
    aura::Window* window) const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window);
}

}  // namespace keyboard
