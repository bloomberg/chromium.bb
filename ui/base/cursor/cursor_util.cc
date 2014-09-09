// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_util.h"

#include "base/logging.h"
#include "skia/ext/image_operations.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"

namespace ui {

void ScaleAndRotateCursorBitmapAndHotpoint(float scale,
                                           gfx::Display::Rotation rotation,
                                           SkBitmap* bitmap,
                                           gfx::Point* hotpoint) {
  switch (rotation) {
    case gfx::Display::ROTATE_0:
      break;
    case gfx::Display::ROTATE_90:
      hotpoint->SetPoint(bitmap->height() - hotpoint->y(), hotpoint->x());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_90_CW);
      break;
    case gfx::Display::ROTATE_180:
      hotpoint->SetPoint(
          bitmap->width() - hotpoint->x(), bitmap->height() - hotpoint->y());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_180_CW);
      break;
    case gfx::Display::ROTATE_270:
      hotpoint->SetPoint(hotpoint->y(), bitmap->width() - hotpoint->x());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_270_CW);
      break;
  }

  if (scale < FLT_EPSILON) {
    NOTREACHED() << "Scale must be larger than 0.";
    scale = 1.0f;
  }

  if (scale == 1.0f)
    return;

  gfx::Size scaled_size = gfx::ToFlooredSize(
      gfx::ScaleSize(gfx::Size(bitmap->width(), bitmap->height()), scale));

  *bitmap = skia::ImageOperations::Resize(
      *bitmap,
      skia::ImageOperations::RESIZE_BETTER,
      scaled_size.width(),
      scaled_size.height());
  *hotpoint = gfx::ToFlooredPoint(gfx::ScalePoint(*hotpoint, scale));
}

void GetImageCursorBitmap(int resource_id,
                          float scale,
                          gfx::Display::Rotation rotation,
                          gfx::Point* hotspot,
                          SkBitmap* bitmap) {
  const gfx::ImageSkia* image =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const gfx::ImageSkiaRep& image_rep = image->GetRepresentation(scale);
  // TODO(oshima): The cursor should use resource scale factor when
  // fractional scale factor is enabled. crbug.com/372212
  (*bitmap) = image_rep.sk_bitmap();
  ScaleAndRotateCursorBitmapAndHotpoint(
      scale / image_rep.scale(), rotation, bitmap, hotspot);
  // |image_rep| is owned by the resource bundle. So we do not need to free it.
}

void GetAnimatedCursorBitmaps(int resource_id,
                              float scale,
                              gfx::Display::Rotation rotation,
                              gfx::Point* hotspot,
                              std::vector<SkBitmap>* bitmaps) {
  // TODO(oshima|tdanderson): Support rotation and fractional scale factor.
  const gfx::ImageSkia* image =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const gfx::ImageSkiaRep& image_rep = image->GetRepresentation(scale);
  SkBitmap bitmap = image_rep.sk_bitmap();
  int frame_width = bitmap.height();
  int frame_height = frame_width;
  int total_width = bitmap.width();
  DCHECK_EQ(total_width % frame_width, 0);
  int frame_count = total_width / frame_width;
  DCHECK_GT(frame_count, 0);

  bitmaps->resize(frame_count);

  for (int frame = 0; frame < frame_count; ++frame) {
    int x_offset = frame_width * frame;
    DCHECK_LE(x_offset + frame_width, total_width);

    SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(
        bitmap, x_offset, 0, frame_width, frame_height);
    DCHECK_EQ(frame_width, cropped.width());
    DCHECK_EQ(frame_height, cropped.height());

    (*bitmaps)[frame] = cropped;
  }
}

}  // namespace ui
