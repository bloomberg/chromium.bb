// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GTK_GTK_SCREEN_UTILS_H_
#define UI_BASE_GTK_GTK_SCREEN_UTILS_H_

#include <gtk/gtk.h>

#include "ui/base/ui_export.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace ui {

// Returns true if the screen is composited, false otherwise.
UI_EXPORT bool IsScreenComposited();

// Get the current location of the mouse cursor relative to the screen.
UI_EXPORT gfx::Point ScreenPoint(GtkWidget* widget);

// Get the current location of the mouse cursor relative to the widget.
UI_EXPORT gfx::Point ClientPoint(GtkWidget* widget);

// Gets the position of a gtk widget in screen coordinates.
UI_EXPORT gfx::Point GetWidgetScreenPosition(GtkWidget* widget);

// Returns the bounds of the specified widget in screen coordinates.
UI_EXPORT gfx::Rect GetWidgetScreenBounds(GtkWidget* widget);

}  // namespace ui

#endif  // UI_BASE_GTK_GTK_SCREEN_UTILS_H_
