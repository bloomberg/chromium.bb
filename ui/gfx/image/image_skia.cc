// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia.h"

#include <limits>
#include <cmath>

#include "base/logging.h"
#include "base/stl_util.h"

namespace gfx {

ImageSkia::ImageSkia(const SkBitmap* bitmap)
    : size_(bitmap->width(), bitmap->height()),
      mip_map_build_pending_(false) {
  CHECK(bitmap);
  // TODO(pkotwicz): Add a CHECK to ensure that !bitmap->isNull()
  bitmaps_.push_back(bitmap);
}

ImageSkia::ImageSkia(const std::vector<const SkBitmap*>& bitmaps)
    : bitmaps_(bitmaps),
      mip_map_build_pending_(false) {
  CHECK(!bitmaps_.empty());
  // TODO(pkotwicz): Add a CHECK to ensure that !bitmap->isNull() for each
  // vector element.
  // Assume that the smallest bitmap represents 1x scale factor.
  for (size_t i = 0; i < bitmaps_.size(); ++i) {
    gfx::Size bitmap_size(bitmaps_[i]->width(), bitmaps_[i]->height());
    if (size_.IsEmpty() || bitmap_size.GetArea() < size_.GetArea())
      size_ = bitmap_size;
  }
}

ImageSkia::~ImageSkia() {
  STLDeleteElements(&bitmaps_);
}

void ImageSkia::BuildMipMap() {
  mip_map_build_pending_ = true;
}

void ImageSkia::DrawToCanvasInt(gfx::Canvas* canvas, int x, int y) {
  SkPaint p;
  DrawToCanvasInt(canvas, x, y, p);
}

void ImageSkia::DrawToCanvasInt(gfx::Canvas* canvas,
                                int x, int y,
                                const SkPaint& paint) {

  if (IsZeroSized())
    return;

  SkMatrix m = canvas->sk_canvas()->getTotalMatrix();
  float scale_x = std::abs(SkScalarToFloat(m.getScaleX()));
  float scale_y = std::abs(SkScalarToFloat(m.getScaleY()));

  const SkBitmap* bitmap = GetBitmapForScale(scale_x, scale_y);

  if (mip_map_build_pending_) {
    const_cast<SkBitmap*>(bitmap)->buildMipMap();
    mip_map_build_pending_ = false;
  }

  float bitmap_scale_x = static_cast<float>(bitmap->width()) / width();
  float bitmap_scale_y = static_cast<float>(bitmap->height()) / height();

  canvas->Save();
  canvas->sk_canvas()->scale(1.0f / bitmap_scale_x,
                             1.0f / bitmap_scale_y);
  canvas->sk_canvas()->drawBitmap(*bitmap, SkFloatToScalar(x * bitmap_scale_x),
                                  SkFloatToScalar(y * bitmap_scale_y));
  canvas->Restore();
}

void ImageSkia::DrawToCanvasInt(gfx::Canvas* canvas,
                                int src_x, int src_y, int src_w, int src_h,
                                int dest_x, int dest_y, int dest_w, int dest_h,
                                bool filter) {
  SkPaint p;
  DrawToCanvasInt(canvas, src_x, src_y, src_w, src_h, dest_x, dest_y,
                  dest_w, dest_h, filter, p);
}

void ImageSkia::DrawToCanvasInt(gfx::Canvas* canvas,
                                int src_x, int src_y, int src_w, int src_h,
                                int dest_x, int dest_y, int dest_w, int dest_h,
                                bool filter,
                                const SkPaint& paint) {
  if (IsZeroSized())
    return;

  SkMatrix m = canvas->sk_canvas()->getTotalMatrix();
  float scale_x = std::abs(SkScalarToFloat(m.getScaleX()));
  float scale_y = std::abs(SkScalarToFloat(m.getScaleY()));

  const SkBitmap* bitmap = GetBitmapForScale(scale_x, scale_y);

  if (mip_map_build_pending_) {
    const_cast<SkBitmap*>(bitmap)->buildMipMap();
    mip_map_build_pending_ = false;
  }

  float bitmap_scale_x = static_cast<float>(bitmap->width()) / width();
  float bitmap_scale_y = static_cast<float>(bitmap->height()) / height();

  canvas->Save();
  canvas->sk_canvas()->scale(1.0f / bitmap_scale_x,
                             1.0f / bitmap_scale_y);
  canvas->DrawBitmapFloat(*bitmap,
                        src_x * bitmap_scale_x, src_y * bitmap_scale_x,
                        src_w * bitmap_scale_x, src_h * bitmap_scale_y,
                        dest_x * bitmap_scale_x, dest_y * bitmap_scale_y,
                        dest_w * bitmap_scale_x, dest_h * bitmap_scale_y,
                        filter, paint);

  canvas->Restore();
}

const SkBitmap* ImageSkia::GetBitmapForScale(float x_scale_factor,
                                             float y_scale_factor) const {
  // Get the desired bitmap width and height given |x_scale_factor|,
  // |y_scale_factor| and |size_| at 1x density.
  float desired_width = size_.width() * x_scale_factor;
  float desired_height = size_.height() * y_scale_factor;

  size_t closest_index = 0;
  float smallest_diff = std::numeric_limits<float>::max();
  for (size_t i = 0; i < bitmaps_.size(); ++i) {
    if (bitmaps_[i]->isNull())
      continue;

    float diff = std::abs(bitmaps_[i]->width() - desired_width) +
        std::abs(bitmaps_[i]->height() - desired_height);
    if (diff < smallest_diff) {
      closest_index = i;
      smallest_diff = diff;
    }
  }
  return bitmaps_[closest_index];
}

}  // namespace gfx
