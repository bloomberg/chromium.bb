// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_UTIL_H_
#define ASH_DISPLAY_DISPLAY_UTIL_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace aura {
class Window;
}

namespace display {
class DisplayManager;
}

namespace gfx {
class Point;
class Rect;
}

namespace ash {
class AshWindowTreeHost;
class MouseWarpController;

// Creates a MouseWarpController for the current display
// configuration. |drag_source| is the window where dragging
// started, or nullptr otherwise.
std::unique_ptr<MouseWarpController> CreateMouseWarpController(
    display::DisplayManager* manager,
    aura::Window* drag_source);

// Creates edge bounds from |bounds_in_screen| that fits the edge
// of the native window for |ash_host|.
ASH_EXPORT gfx::Rect GetNativeEdgeBounds(AshWindowTreeHost* ash_host,
                                         const gfx::Rect& bounds_in_screen);

// Moves the cursor to the point inside the |ash_host| that is closest to
// the point_in_screen, which may be outside of the root window.
// |update_last_loation_now| is used for the test to update the mouse
// location synchronously.
void MoveCursorTo(AshWindowTreeHost* ash_host,
                  const gfx::Point& point_in_screen,
                  bool update_last_location_now);

// Shows the notification message for display related issues, and optionally
// adds a button to send a feedback report.
void ShowDisplayErrorNotification(const base::string16& message,
                                  bool allow_feedback);

ASH_EXPORT base::string16 GetDisplayErrorNotificationMessageForTest();

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_UTIL_H_
