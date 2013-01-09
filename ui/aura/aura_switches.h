// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_AURA_SWITCHES_H_
#define UI_AURA_AURA_SWITCHES_H_

#include "ui/aura/aura_export.h"

namespace switches {

// Please keep alphabetized.

// Sets a window size, optional position, and optional scale factor.
// "1024x768" creates a window of size 1024x768.
// "100+200-1024x768" positions the window at 100,200.
// "1024x768*2" sets the scale factor to 2 for a high DPI display.
AURA_EXPORT extern const char kAuraHostWindowSize[];

AURA_EXPORT extern const char kAuraHostWindowUseFullscreen[];

}  // namespace switches

#endif  // UI_AURA_AURA_SWITCHES_H_
