// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_export.h"

namespace gfx {

namespace internal {
class ImageSkiaStorage;
}  // namespace internal

// Container for the same image at different densities, similar to NSImage.
// Image height and width are in DIP (Device Indepent Pixel) coordinates.
//
// ImageSkia should be used whenever possible instead of SkBitmap.
// Functions that mutate the image should operate on the SkBitmap returned
// from ImageSkia::GetBitmapForScale, not on ImageSkia.
//
// ImageSkia is cheap to copy and intentionally supports copy semantics.
class UI_EXPORT ImageSkia {
 public:
  // Creates instance with no bitmaps.
  ImageSkia();

  // Adds ref to passed in bitmap.
  // DIP width and height are set based on scale factor of 1x.
  // TODO(pkotwicz): This is temporary till conversion to gfx::ImageSkia is
  // done.
  ImageSkia(const SkBitmap& bitmap);

  // Adds ref to passed in bitmap.
  // DIP width and height are set based on |dip_scale_factor|.
  ImageSkia(const SkBitmap& bitmap, float dip_scale_factor);

  // Copies a reference to |other|'s storage.
  ImageSkia(const ImageSkia& other);

  // Copies a reference to |other|'s storage.
  ImageSkia& operator=(const ImageSkia& other);

  // Converts from SkBitmap.
  // Adds ref to passed in bitmap.
  // DIP width and height are set based on scale factor of 1x.
  // TODO(pkotwicz): This is temporary till conversion to gfx::ImageSkia is
  // done.
  ImageSkia& operator=(const SkBitmap& other);

  // Converts to SkBitmap.
  // TODO(pkotwicz): This is temporary till conversion to gfx::ImageSkia is
  // done.
  operator SkBitmap&() const;

  ~ImageSkia();

  // Adds |bitmap| for |dip_scale_factor| to bitmaps contained by this object.
  // Adds ref to passed in bitmap.
  // DIP width and height are set based on |dip_scale_factor|.
  void AddBitmapForScale(const SkBitmap& bitmap, float dip_scale_factor);

  // Returns the bitmap whose density best matches |x_scale_factor| and
  // |y_scale_factor|.
  // Returns a null bitmap if object contains no bitmaps.
  // |bitmap_scale_factor| is set to the scale factor of the returned bitmap.
  const SkBitmap& GetBitmapForScale(float x_scale_factor,
                                    float y_scale_factor,
                                    float* bitmap_scale_factor) const;

  // Returns true if object is null or |size_| is empty.
  bool empty() const;

  // Returns true if this is a null object.
  // TODO(pkotwicz): Merge this function into empty().
  bool isNull() const { return storage_ == NULL; }

  // Width and height of image in DIP coordinate system.
  int width() const;
  int height() const;

  // Wrapper function for SkBitmap::extractBitmap.
  // Operates on bitmap at index 0 if available.
  // TODO(pkotwicz): This is temporary till conversion to gfx::ImageSkia is
  // done.
  bool extractSubset(ImageSkia* dst, SkIRect& subset) const;

  // Returns pointer to an SkBitmap contained by this object.
  // TODO(pkotwicz): This is temporary till conversion to gfx::ImageSkia is
  // done.
  const SkBitmap* bitmap() const;

  // Returns a vector with the SkBitmaps contained in this object.
  const std::vector<SkBitmap> bitmaps() const;

 private:
  // Initialize ImageStorage with passed in parameters.
  // If |bitmap.isNull()|, ImageStorage is set to NULL.
  // Scale factor is set based on default scale factor of 1x.
  // TODO(pkotwicz): This is temporary till conversion to gfx::ImageSkia is
  // done.
  void Init(const SkBitmap& bitmap);

  // Initialize ImageStorage with passed in parameters.
  // If |bitmap.isNull()|, ImageStorage is set to NULL.
  void Init(const SkBitmap& bitmap, float scale_factor);

  // A refptr so that ImageRepSkia can be copied cheaply.
  scoped_refptr<internal::ImageSkiaStorage> storage_;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_H_
