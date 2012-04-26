// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_DESKTOP_SCREEN_H_
#define UI_AURA_DESKTOP_DESKTOP_SCREEN_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace gfx {
class ScreenImpl;
}

namespace aura {

// Creates a ScreenImpl that represents the screen of the environment that hosts
// a RootWindowHost. Caller owns the result.
AURA_EXPORT gfx::ScreenImpl* CreateDesktopScreen();

}  // namespace aura

#endif  // UI_AURA_DESKTOP_DESKTOP_SCREEN_H_
