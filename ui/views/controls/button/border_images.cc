// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/border_images.h"

#include "base/logging.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/rect.h"

#define BORDER_IMAGES(x) x ## _TOP_LEFT,    x ## _TOP,    x ## _TOP_RIGHT, \
                         x ## _LEFT,        x ## _CENTER, x ## _RIGHT, \
                         x ## _BOTTOM_LEFT, x ## _BOTTOM, x ## _BOTTOM_RIGHT,

namespace views {

// static
const int BorderImages::kHot[] = { BORDER_IMAGES(IDR_TEXTBUTTON_HOVER) };
// static
const int BorderImages::kPushed[] = { BORDER_IMAGES(IDR_TEXTBUTTON_PRESSED) };

BorderImages::BorderImages() {}

BorderImages::BorderImages(const int image_ids[]) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  top_left = *rb.GetImageSkiaNamed(image_ids[0]);
  top = *rb.GetImageSkiaNamed(image_ids[1]);
  top_right = *rb.GetImageSkiaNamed(image_ids[2]);
  left = *rb.GetImageSkiaNamed(image_ids[3]);
  center = *rb.GetImageSkiaNamed(image_ids[4]);
  right = *rb.GetImageSkiaNamed(image_ids[5]);
  bottom_left = *rb.GetImageSkiaNamed(image_ids[6]);
  bottom = *rb.GetImageSkiaNamed(image_ids[7]);
  bottom_right = *rb.GetImageSkiaNamed(image_ids[8]);
}

BorderImages::~BorderImages() {}

void BorderImages::Paint(const gfx::Rect& rect, gfx::Canvas* canvas) const {
  DCHECK(!top_left.isNull());

  // Images must share widths by column and heights by row as depicted below.
  //     x0   x1   x2   x3
  // y0__|____|____|____|
  // y1__|_tl_|_t__|_tr_|
  // y2__|_l__|_c__|_r__|
  // y3__|_bl_|_b__|_br_|
  const int x[] = { rect.x(), rect.x() + top_left.width(),
                    rect.right() - top_right.width(), rect.right() };
  const int y[] = { rect.y(), rect.y() + top_left.height(),
                    rect.bottom() - bottom_left.height(), rect.bottom() };

  canvas->DrawImageInt(top_left, x[0], y[0]);
  canvas->TileImageInt(top, x[1], y[0], x[2] - x[1], y[1] - y[0]);
  canvas->DrawImageInt(top_right, x[2], y[0]);
  canvas->TileImageInt(left, x[0], y[1], x[1] - x[0], y[2] - y[1]);
  canvas->DrawImageInt(center, 0, 0, center.width(), center.height(),
                       x[1], y[1], x[2] - x[1], y[2] - y[1], false);
  canvas->TileImageInt(right, x[2], y[1], x[3] - x[2], y[2] - y[1]);
  canvas->DrawImageInt(bottom_left, 0, y[2]);
  canvas->TileImageInt(bottom, x[1], y[2], x[2] - x[1], y[3] - y[2]);
  canvas->DrawImageInt(bottom_right, x[2], y[2]);
}

}  // namespace views
