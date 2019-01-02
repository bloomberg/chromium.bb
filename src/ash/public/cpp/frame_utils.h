// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FRAME_UTILS_H_
#define ASH_PUBLIC_CPP_FRAME_UTILS_H_

#include "ash/public/cpp/ash_public_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class ImageSkia;
class Point;
class Rect;
}  // namespace gfx

namespace views {
class NonClientFrameView;
}

namespace ash {

// Returns the HitTestCompat for the specified point.
ASH_PUBLIC_EXPORT int FrameBorderNonClientHitTest(
    views::NonClientFrameView* view,
    const gfx::Point& point_in_widget);

// Paints the frame header images and background color for custom themes (i.e.
// browser themes) into a canvas.
ASH_PUBLIC_EXPORT void PaintThemedFrame(
    gfx::Canvas* canvas,
    const gfx::ImageSkia& frame_image,
    const gfx::ImageSkia& frame_overlay_image,
    SkColor background_color,
    const gfx::Rect& bounds,
    int image_inset_x,
    int image_inset_y,
    int alpha);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FRAME_UTILS_H_
