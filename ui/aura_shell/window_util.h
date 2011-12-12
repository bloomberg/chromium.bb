// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WINDOW_UTIL_H_
#define UI_AURA_SHELL_WINDOW_UTIL_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class Window;
}

namespace aura_shell {

// Returns true if |window| is in the maximized state.
AURA_SHELL_EXPORT bool IsWindowMaximized(aura::Window* window);

// Convenience setters/getters for |aura::kRootWindowActiveWindow|.
AURA_SHELL_EXPORT void ActivateWindow(aura::Window* window);
AURA_SHELL_EXPORT void DeactivateWindow(aura::Window* window);
AURA_SHELL_EXPORT bool IsActiveWindow(aura::Window* window);
AURA_SHELL_EXPORT aura::Window* GetActiveWindow();

// Retrieves the activatable window for |window|. If |window| is activatable,
// this will just return it, otherwise it will climb the parent/transient parent
// chain looking for a window that is activatable, per the ActivationController.
// If you're looking for a function to get the activatable "top level" window,
// this is probably what you're looking for.
AURA_SHELL_EXPORT aura::Window* GetActivatableWindow(aura::Window* window);

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WINDOW_UTIL_H_
