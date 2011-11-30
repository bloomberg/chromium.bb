// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_WINDOW_UTIL_H_
#define UI_AURA_SHELL_WINDOW_UTIL_H_
#pragma once

namespace aura {
class Window;
}

namespace aura_shell {

// Returns true if |window| is in the maximized state.
bool IsWindowMaximized(aura::Window* window);

}  // namespace aura_shell

#endif  // UI_AURA_SHELL_WINDOW_UTIL_H_
