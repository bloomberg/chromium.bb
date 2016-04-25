// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_SCREEN_H_
#define UI_DISPLAY_SCREEN_H_

#include "ui/gfx/screen.h"

namespace display {

// TODO(oshima): move the gfx::Screen to display::Screen.
using Display = gfx::Display;
using DisplayObserver = gfx::DisplayObserver;
using Screen = gfx::Screen;

}  // display

#endif  // UI_DISPLAY_SCREEN_H_
