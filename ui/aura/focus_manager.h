// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_FOCUS_MANAGER_H_
#define UI_AURA_FOCUS_MANAGER_H_
#pragma once

#include "base/basictypes.h"

namespace aura {
class Window;

namespace internal {

// An interface implemented by the RootWindow to expose the focused window and
// allow for it to be changed.
class FocusManager {
 public:
  // Sets the currently focused window. Before the currently focused window is
  // changed, the previous focused window's delegate is sent a blur
  // notification, and after it is changed the new focused window is sent a
  // focused notification. Nothing happens if |window| and GetFocusedWindow()
  // match.
  virtual void SetFocusedWindow(Window* window) = 0;

  // Returns the currently focused window or NULL if there is none.
  virtual Window* GetFocusedWindow() = 0;

 protected:
  virtual ~FocusManager() {}
};

}  // namespace internal
}  // namespace aura

#endif  // UI_AURA_FOCUS_MANAGER_H_
