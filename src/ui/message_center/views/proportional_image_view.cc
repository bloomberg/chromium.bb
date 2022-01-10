// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/proportional_image_view.h"

#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/message_center/message_center_style.h"

namespace message_center {
namespace {
constexpr int kRoundedCornerRadius = 8;
}

ProportionalImageView::ProportionalImageView(const gfx::Size& view_size) {
  SetPreferredSize(view_size);
}

ProportionalImageView::~ProportionalImageView() {}

void ProportionalImageView::SetImage(const gfx::ImageSkia& image,
                                     const gfx::Size& max_image_size,
                                     bool apply_rounded_corners) {
  apply_rounded_corners_ = apply_rounded_corners;
  image_ = image;
  max_image_size_ = max_image_size;
  SchedulePaint();
}

void ProportionalImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size draw_size = GetImageDrawingSize();
  if (draw_size.IsEmpty())
    return;

  gfx::Rect draw_bounds = GetContentsBounds();
  draw_bounds.ClampToCenteredSize(draw_size);

  gfx::ImageSkia image =
      (image_.size() == draw_size)
          ? image_
          : gfx::ImageSkiaOperations::CreateResizedImage(
                image_, skia::ImageOperations::RESIZE_BEST, draw_size);

  if (apply_rounded_corners_) {
    SkPath path;
    const SkScalar corner_radius = SkIntToScalar(kRoundedCornerRadius);
    const SkScalar kRadius[8] = {corner_radius, corner_radius, corner_radius,
                                 corner_radius, corner_radius, corner_radius,
                                 corner_radius, corner_radius};
    path.addRoundRect(gfx::RectToSkRect(draw_bounds), kRadius);

    cc::PaintFlags flags;
    flags.setAntiAlias(true);

    canvas->DrawImageInPath(image, draw_bounds.x(), draw_bounds.y(), path,
                            flags);
    return;
  }

  canvas->DrawImageInt(image, draw_bounds.x(), draw_bounds.y());
}

gfx::Size ProportionalImageView::GetImageDrawingSize() {
  if (!GetVisible())
    return gfx::Size();

  gfx::Size max_size = max_image_size_;
  max_size.SetToMin(GetContentsBounds().size());
  return GetImageSizeForContainerSize(max_size, image_.size());
}

BEGIN_METADATA(ProportionalImageView, views::View)
END_METADATA

}  // namespace message_center
