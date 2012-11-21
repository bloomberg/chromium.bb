// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ROUNDED_RECT_PAINTER_H_
#define UI_VIEWS_ROUNDED_RECT_PAINTER_H_

#include "ui/gfx/rect.h"
#include "ui/views/painter.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class Path;
class Size;
}

namespace views {

// Painter to draw a border with rounded corners.
class VIEWS_EXPORT RoundRectPainter : public Painter {
 public:
  explicit RoundRectPainter(SkColor border_color);
  virtual ~RoundRectPainter();

  // Painter overrides:
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE;

  // Creates |path| for the same rectangle Paint() will draw.
  static void CreateRoundRectPath(const gfx::Rect& bounds, gfx::Path* path);

 private:
  const SkColor border_color_;

  DISALLOW_COPY_AND_ASSIGN(RoundRectPainter);
};

}  // namespace views

#endif  // UI_VIEWS_ROUNDED_RECT_PAINTER_H_
