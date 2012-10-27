// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BORDER_IMAGES_H_
#define UI_VIEWS_CONTROLS_BUTTON_BORDER_IMAGES_H_

#include "ui/gfx/image/image_skia.h"
#include "ui/views/views_export.h"

namespace gfx {
class Canvas;
class Rect;
}

namespace views {

// BorderImages stores and paints the nine images comprising a button border.
// TODO(msw): Merge common "nine-box" code with BubbleBorder, ImagePainter, etc.
// TODO(msw): Stitch border image assets together and use ImagePainter.
struct VIEWS_EXPORT BorderImages {
  // The default hot and pushed button image IDs; normal has none by default.
  static const int kHot[];
  static const int kPushed[];

  BorderImages();
  // |image_ids| must contain 9 image IDs matching the member order below.
  explicit BorderImages(const int image_ids[]);
  ~BorderImages();

  // Paint the images on |canvas| within |rect|'s dimensions.
  void Paint(const gfx::Rect& rect, gfx::Canvas* canvas) const;

  gfx::ImageSkia top_left;
  gfx::ImageSkia top;
  gfx::ImageSkia top_right;
  gfx::ImageSkia left;
  gfx::ImageSkia center;
  gfx::ImageSkia right;
  gfx::ImageSkia bottom_left;
  gfx::ImageSkia bottom;
  gfx::ImageSkia bottom_right;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_BORDER_IMAGES_H_
