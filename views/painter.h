// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_PAINTER_H_
#define VIEWS_PAINTER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/views_export.h"

namespace gfx {
class Canvas;
class Insets;
}
class SkBitmap;

namespace views {

// Painter, as the name implies, is responsible for painting in a particular
// region. Think of Painter as a Border or Background that can be painted
// in any region of a View.
class VIEWS_EXPORT Painter {
 public:
  // A convenience method for painting a Painter in a particular region.
  // This translates the canvas to x/y and paints the painter.
  static void PaintPainterAt(int x, int y, int w, int h,
                             gfx::Canvas* canvas, Painter* painter);

  // Creates a painter that draws a gradient between the two colors.
  static Painter* CreateHorizontalGradient(SkColor c1, SkColor c2);
  static Painter* CreateVerticalGradient(SkColor c1, SkColor c2);

  // Creates a painter that divides |image| into nine regions. The four corners
  // are rendered at the size specified in insets (for example, the upper
  // left corners is rendered at 0x0 with a size of
  // insets.left()xinsets.right()). The four edges are stretched to fill the
  // destination size.
  // Ownership is passed to the caller.
  static Painter* CreateImagePainter(const SkBitmap& image,
                                     const gfx::Insets& insets,
                                     bool paint_center);

  virtual ~Painter() {}

  // Paints the painter in the specified region.
  virtual void Paint(int w, int h, gfx::Canvas* canvas) = 0;
};

// HorizontalPainter paints 3 images into a box: left, center and right. The
// left and right images are drawn to size at the left/right edges of the
// region. The center is tiled in the remaining space. All images must have the
// same height.
class VIEWS_EXPORT HorizontalPainter : public Painter {
 public:
  // Constructs a new HorizontalPainter loading the specified image names.
  // The images must be in the order left, right and center.
  explicit HorizontalPainter(const int image_resource_names[]);

  virtual ~HorizontalPainter() {}

  // Paints the images.
  virtual void Paint(int w, int h, gfx::Canvas* canvas) OVERRIDE;

  // Height of the images.
  int height() const { return height_; }

 private:
  // The image chunks.
  enum BorderElements {
    LEFT,
    CENTER,
    RIGHT
  };

  // The height.
  int height_;
  // NOTE: the images are owned by ResourceBundle. Don't free them.
  SkBitmap* images_[3];

  DISALLOW_COPY_AND_ASSIGN(HorizontalPainter);
};

}  // namespace views

#endif  // VIEWS_PAINTER_H_
