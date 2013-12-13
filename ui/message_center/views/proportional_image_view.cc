// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/proportional_image_view.h"

#include "ui/gfx/canvas.h"
#include "ui/message_center/message_center_style.h"

namespace message_center {

ProportionalImageView::ProportionalImageView(const gfx::ImageSkia& image)
    : image_(image) {
}

ProportionalImageView::~ProportionalImageView() {
}

gfx::Size ProportionalImageView::GetPreferredSize() {
  gfx::Insets insets = GetInsets();
  gfx::Rect rect = gfx::Rect(GetImageSizeForWidth(image_.width()));
  rect.Inset(-insets);
  return rect.size();
}

int ProportionalImageView::GetHeightForWidth(int width) {
  // The border will count against the width available for the image
  // and towards the height taken by the image.
  gfx::Insets insets = GetInsets();
  int inset_width = width - insets.width();
  return GetImageSizeForWidth(inset_width).height() + insets.height();
}

void ProportionalImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size draw_size(GetImageSizeForWidth(width() - GetInsets().width()));
  if (!draw_size.IsEmpty()) {
    gfx::Rect draw_bounds = GetContentsBounds();
    draw_bounds.ClampToCenteredSize(draw_size);

    gfx::Size image_size(image_.size());
    if (image_size == draw_size) {
      canvas->DrawImageInt(image_, draw_bounds.x(), draw_bounds.y());
    } else {
      // Resize case
      SkPaint paint;
      paint.setFilterBitmap(true);
      canvas->DrawImageInt(image_, 0, 0,
                           image_size.width(), image_size.height(),
                           draw_bounds.x(), draw_bounds.y(),
                           draw_size.width(), draw_size.height(),
                           true, paint);
    }
  }
}

gfx::Size ProportionalImageView::GetImageSizeForWidth(int width) {
  gfx::Size size = visible() ? image_.size() : gfx::Size();
  return message_center::GetImageSizeForWidth(width, size);
}

}  // namespace message_center
