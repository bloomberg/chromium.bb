// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_UTIL_H_
#define UI_VIEWS_WIN_HWND_UTIL_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace views {

class View;
class Widget;

// Returns the HWND for the specified View.
VIEWS_EXPORT HWND HWNDForView(View* view);

// Returns the HWND for the specified Widget.
VIEWS_EXPORT HWND HWNDForWidget(Widget* widget);

// Returns the HWND for the specified NativeWindow.
VIEWS_EXPORT HWND HWNDForNativeWindow(gfx::NativeWindow window);

}

#endif  // UI_VIEWS_WIN_HWND_UTIL_H_
