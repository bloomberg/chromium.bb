// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_DISPLAY_UTIL_H_
#define UI_KEYBOARD_DISPLAY_UTIL_H_

#include "ui/aura/window.h"
#include "ui/display/display.h"

namespace keyboard {

// Helper class for querying information about the screen.
class DisplayUtil {
 public:
  DisplayUtil();

  int64_t GetNearestDisplayIdToWindow(aura::Window* window) const;
  display::Display GetNearestDisplayToWindow(aura::Window* window) const;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_DISPLAY_UTIL_H_
