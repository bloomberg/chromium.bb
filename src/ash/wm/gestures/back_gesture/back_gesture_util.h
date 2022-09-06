// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_UTIL_H_
#define ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_UTIL_H_

#include "cc/paint/paint_flags.h"

namespace gfx {
class Canvas;
class PointF;
class Rect;
}  // namespace gfx

namespace ash {

// Paints the circular shaped highlight border onto `canvas`.
void DrawCircleHighlightBorder(gfx::Canvas* canvas,
                               const gfx::PointF& circle_center,
                               int radius);

// Paints the round rectangular shaped highlight border onto `canvas`.
void DrawRoundRectHighlightBorder(gfx::Canvas* canvas,
                                  const gfx::Rect& bounds,
                                  int corner_radius);

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_UTIL_H_