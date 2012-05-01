// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_HWND_UTILS_H_
#define UI_VIEWS_WIDGET_WIDGET_HWND_UTILS_H_
#pragma once

#include <windows.h>

// Functions shared by native_widget_win.cc and widget_message_filter.cc:

namespace views {

// Returns true if the WINDOWPOS data provided indicates the client area of
// the window may have changed size. This can be caused by the window being
// resized or its frame changing.
bool DidClientAreaSizeChange(const WINDOWPOS* window_pos);

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_HWND_UTILS_H_
