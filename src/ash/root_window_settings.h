// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_SETTINGS_H_
#define ASH_ROOT_WINDOW_SETTINGS_H_

#include <stdint.h>

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

class RootWindowController;

// Per root window information should be stored here
// instead of using plain aura root window property because
// it can prevent mis-using on non root window.
struct RootWindowSettings {
  RootWindowSettings();

  // ID of the display associated with the root window.
  int64_t display_id;

  // RootWindowController for the root window. This may be NULL
  // for the root window used for mirroring.
  RootWindowController* controller;
};

// Initializes and returns RootWindowSettings for |root|.
// It is owned by the |root|.
ASH_EXPORT RootWindowSettings* InitRootWindowSettings(aura::Window* root);

// Returns the RootWindowSettings for |root|.
ASH_EXPORT RootWindowSettings* GetRootWindowSettings(aura::Window* root);

// const version of GetRootWindowSettings.
ASH_EXPORT const RootWindowSettings* GetRootWindowSettings(
    const aura::Window* root);

}  // namespace ash

#endif  // ASH_ROOT_WINDOW_SETTINGS_H_
