// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"

namespace gfx {

// Container for the same image at different densities, similar to NSImage.
// Smallest image is assumed to represent 1x density.

// ImageSkia should be used for caching and drawing images of different
// densities. It should not be used as an SkBitmap wrapper.
class UI_EXPORT ImageSkia {
 public:
  explicit ImageSkia(const SkBitmap* bitmap);
  explicit ImageSkia(const std::vector<const SkBitmap*>& bitmaps);
  ~ImageSkia();

  // Build mipmap at time of next call to |DrawToCanvasInt|.
  void BuildMipMap();

  // Draws the image with the origin at the specified location. The upper left
  // corner of the image is rendered at the specified location.
  void DrawToCanvasInt(Canvas* canvas, int x, int y);

  // Draws the image with the origin at the specified location, using the
  // specified paint. The upper left corner of the image is rendered at the
  // specified location.
  void DrawToCanvasInt(Canvas* canvas,
                       int x, int y,
                       const SkPaint& paint);

  // Draws a portion of the image in the specified location. The src parameters
  // correspond to the region of the image to draw in the region defined
  // by the dest coordinates.
  //
  // If the width or height of the source differs from that of the destination,
  // the image will be scaled. When scaling down, it is highly recommended
  // that you call BuildMipMap() on your image to ensure that it has
  // a mipmap, which will result in much higher-quality output. Set |filter| to
  // use filtering for bitmaps, otherwise the nearest-neighbor algorithm is used
  // for resampling.
  //
  // An optional custom SkPaint can be provided.
  void DrawToCanvasInt(Canvas* canvas,
                       int src_x, int src_y, int src_w, int src_h,
                       int dest_x, int dest_y, int dest_w, int dest_h,
                       bool filter);
  void DrawToCanvasInt(Canvas* canvas,
                       int src_x, int src_y, int src_w, int src_h,
                       int dest_x, int dest_y, int dest_w, int dest_h,
                       bool filter,
                       const SkPaint& paint);

  // Returns true if |size_| is empty.
  bool IsZeroSized() const { return size_.IsEmpty(); }

  // Width and height of image in DIP coordinate system.
  int width() const { return size_.width(); }
  int height() const { return size_.height(); }

  // Returns a vector with the SkBitmaps contained in this object.
  const std::vector<const SkBitmap*>& bitmaps() const { return bitmaps_; }

 private:
  // Returns the bitmap whose density best matches |x_scale_factor| and
  // |y_scale_factor|.
  const SkBitmap* GetBitmapForScale(float x_scale_factor,
                                    float y_scale_factor) const;

  std::vector<const SkBitmap*> bitmaps_;
  gfx::Size size_;
  bool mip_map_build_pending_;

  DISALLOW_COPY_AND_ASSIGN(ImageSkia);
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_H_
