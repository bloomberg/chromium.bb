// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_HWND_UTIL_H_
#define UI_BASE_WIN_HWND_UTIL_H_
#pragma once

#include <windows.h>

#include "base/string16.h"
#include "ui/ui_api.h"

namespace gfx {
class Size;
}

namespace ui {

// A version of the GetClassNameW API that returns the class name in an
// string16. An empty result indicates a failure to get the class name.
UI_API string16 GetClassName(HWND hwnd);

// Useful for subclassing a HWND.  Returns the previous window procedure.
UI_API WNDPROC SetWindowProc(HWND hwnd, WNDPROC wndproc);

// Pointer-friendly wrappers around Get/SetWindowLong(..., GWLP_USERDATA, ...)
// Returns the previously set value.
UI_API void* SetWindowUserData(HWND hwnd, void* user_data);
UI_API void* GetWindowUserData(HWND hwnd);

// Returns true if the specified window is the current active top window or one
// of its children.
UI_API bool DoesWindowBelongToActiveWindow(HWND window);

// Sizes the window to have a client or window size (depending on the value of
// |pref_is_client|) of pref, then centers the window over parent, ensuring the
// window fits on screen.
UI_API void CenterAndSizeWindow(HWND parent,
                                HWND window,
                                const gfx::Size& pref,
                                bool pref_is_client);

// If |hwnd| is NULL logs various thing and CHECKs. Invoke right after calling
// CreateWindow.
UI_API void CheckWindowCreated(HWND hwnd);

// Shows the system menu for |window| and sends the selected command (if the
// user selected something.
UI_API void ShowSystemMenu(HWND window, int screen_x, int screen_y);

}  // namespace ui

#endif  // UI_BASE_WIN_HWND_UTIL_H_
