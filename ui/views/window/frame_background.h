// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_FRAME_BACKGROUND_H_
#define UI_VIEWS_WINDOW_FRAME_BACKGROUND_H_
#pragma once

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/views_export.h"

class SkBitmap;
namespace gfx {
class Canvas;
}

namespace views {

class View;

// FrameBackground handles painting for all the various window frames we
// support in Chrome. It intends to consolidate paint code that historically
// was copied. One frame to rule them all!
class VIEWS_EXPORT FrameBackground {
 public:
  FrameBackground();
  ~FrameBackground();

  // Sets the color to draw under the frame bitmaps.
  void set_frame_color(SkColor color) { frame_color_ = color; }

  // Sets the theme bitmap for the top of the window.  May be NULL.
  // Memory is owned by the caller.
  void set_theme_bitmap(SkBitmap* bitmap) { theme_bitmap_ = bitmap; }

  // Sets an image that overlays the top window bitmap.  Usually used to add
  // edge highlighting to provide the illusion of depth.  May be NULL.
  // Memory is owned by the caller.
  void set_theme_overlay_bitmap(SkBitmap* bitmap) {
    theme_overlay_bitmap_ = bitmap;
  }

  // Sets the height of the top area to fill with the default frame color,
  // which must extend behind the tab strip.
  void set_top_area_height(int height) { top_area_height_ = height; }

  // Only used if we have an overlay image for the theme.
  void set_theme_background_y(int y) { theme_background_y_ = y; }

  // Vertical offset for theme image when drawing maximized.
  void set_maximized_top_offset(int offset) { maximized_top_offset_ = offset; }

  // Sets images used when drawing the sides of the frame.
  // Caller owns the memory.
  void SetSideImages(SkBitmap* left,
                     SkBitmap* top,
                     SkBitmap* right,
                     SkBitmap* bottom);

  // Sets images used when drawing the corners of the frame.
  // Caller owns the memory.
  void SetCornerImages(SkBitmap* top_left,
                       SkBitmap* top_right,
                       SkBitmap* bottom_left,
                       SkBitmap* bottom_right);

  // Sets attributes to paint top-left and top-right corners for maximized
  // windows.  Use 0 and NULL if you don't want special corners.
  // TODO(jamescook): This is the remnant of a ChromeOS window hack, and should
  // be removed.
  void SetMaximizedCorners(SkBitmap* top_left,
                           SkBitmap* top_right,
                           int top_offset);

  // Paints the border for a standard, non-maximized window.  Also paints the
  // background of the title bar area, since the top frame border and the
  // title bar background are a contiguous component.
  void PaintRestored(gfx::Canvas* canvas, View* view) const;

  // Paints the border for a maximized window, which does not include the
  // window edges.
  void PaintMaximized(gfx::Canvas* canvas, View* view) const;

 private:
  // Fills the frame area with the frame color.
  void PaintFrameColor(gfx::Canvas* canvas, View* view) const;

  SkColor frame_color_;
  SkBitmap* theme_bitmap_;
  SkBitmap* theme_overlay_bitmap_;
  int top_area_height_;

  // Images for the sides of the frame.
  SkBitmap* left_edge_;
  SkBitmap* top_edge_;
  SkBitmap* right_edge_;
  SkBitmap* bottom_edge_;

  // Images for the corners of the frame.
  SkBitmap* top_left_corner_;
  SkBitmap* top_right_corner_;
  SkBitmap* bottom_left_corner_;
  SkBitmap* bottom_right_corner_;

  // Attributes for maximized window painting.
  // TODO(jamescook): Remove all these.
  SkBitmap* maximized_top_left_;
  SkBitmap* maximized_top_right_;
  int maximized_top_offset_;
  int theme_background_y_;

  DISALLOW_COPY_AND_ASSIGN(FrameBackground);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_FRAME_BACKGROUND_H_
