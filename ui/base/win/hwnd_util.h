// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_HWND_UTIL_H_
#define UI_BASE_WIN_HWND_UTIL_H_
#pragma once

#include <windows.h>

#include "base/string16.h"

namespace gfx {
class Size;
}

namespace ui {

// A version of the GetClassNameW API that returns the class name in an
// string16. An empty result indicates a failure to get the class name.
string16 GetClassName(HWND hwnd);

// Useful for subclassing a HWND.  Returns the previous window procedure.
WNDPROC SetWindowProc(HWND hwnd, WNDPROC wndproc);

// Pointer-friendly wrappers around Get/SetWindowLong(..., GWLP_USERDATA, ...)
// Returns the previously set value.
void* SetWindowUserData(HWND hwnd, void* user_data);
void* GetWindowUserData(HWND hwnd);

// Returns true if the specified window is the current active top window or one
// of its children.
bool DoesWindowBelongToActiveWindow(HWND window);

// Sizes the window to have a client or window size (depending on the value of
// |pref_is_client|) of pref, then centers the window over parent, ensuring the
// window fits on screen.
void CenterAndSizeWindow(HWND parent,
                         HWND window,
                         const gfx::Size& pref,
                         bool pref_is_client);

// Returns true if we are on Windows Vista or greater and composition is
// enabled.
bool ShouldUseVistaFrame();

}  // namespace ui

#endif  // UI_BASE_WIN_HWND_UTIL_H_
