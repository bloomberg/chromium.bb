// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_BORDER_IMAGES_H_
#define UI_VIEWS_CONTROLS_BUTTON_BORDER_IMAGES_H_

#include "ui/gfx/image/image_skia.h"
#include "ui/views/painter.h"
#include "ui/views/views_export.h"

#define BORDER_IMAGES(x) x ## _TOP_LEFT,    x ## _TOP,    x ## _TOP_RIGHT, \
                         x ## _LEFT,        x ## _CENTER, x ## _RIGHT, \
                         x ## _BOTTOM_LEFT, x ## _BOTTOM, x ## _BOTTOM_RIGHT,

namespace views {

// BorderImages stores and paints the nine images comprising a button border.
// TODO(msw): Merge common "nine-box" code with BubbleBorder, ImagePainter, etc.
// TODO(msw): Stitch border image assets together and use ImagePainter.
class VIEWS_EXPORT BorderImages : public Painter {
 public:
  // The default hot and pushed button image IDs; normal has none by default.
  static const int kHot[];
  static const int kPushed[];

  BorderImages();
  // |image_ids| must contain 9 image IDs matching the member order below.
  explicit BorderImages(const int image_ids[]);
  virtual ~BorderImages();

  // Returns true if the images are empty.
  bool IsEmpty() const;

  // Overridden from Painter:
  virtual void Paint(gfx::Canvas* canvas, const gfx::Size& size) OVERRIDE;

 private:
  gfx::ImageSkia top_left_;
  gfx::ImageSkia top_;
  gfx::ImageSkia top_right_;
  gfx::ImageSkia left_;
  gfx::ImageSkia center_;
  gfx::ImageSkia right_;
  gfx::ImageSkia bottom_left_;
  gfx::ImageSkia bottom_;
  gfx::ImageSkia bottom_right_;
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_BORDER_IMAGES_H_
