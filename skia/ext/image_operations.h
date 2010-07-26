// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_IMAGE_OPERATIONS_H_
#define SKIA_EXT_IMAGE_OPERATIONS_H_
#pragma once

class SkBitmap;
struct SkIRect;

namespace skia {

class ImageOperations {
 public:
  enum ResizeMethod {
    // Box filter. This is a weighted average of all of the pixels touching
    // the destination pixel. For enlargement, this is nearest neighbor.
    //
    // You probably don't want this, it is here for testing since it is easy to
    // compute. Use RESIZE_LANCZOS3 instead.
    RESIZE_BOX,

    // 3-cycle Lanczos filter. This is tall in the middle, goes negative on
    // each side, then oscillates 2 more times. It gives nice sharp edges.
    RESIZE_LANCZOS3,

    // Lanczos filter + subpixel interpolation. If subpixel rendering is not
    // appropriate we automatically fall back to Lanczos.
    RESIZE_SUBPIXEL,
  };

  // Resizes the given source bitmap using the specified resize method, so that
  // the entire image is (dest_size) big. The dest_subset is the rectangle in
  // this destination image that should actually be returned.
  //
  // The output image will be (dest_subset.width(), dest_subset.height()). This
  // will save work if you do not need the entire bitmap.
  //
  // The destination subset must be smaller than the destination image.
  static SkBitmap Resize(const SkBitmap& source,
                         ResizeMethod method,
                         int dest_width, int dest_height,
                         const SkIRect& dest_subset);

  // Alternate version for resizing and returning the entire bitmap rather than
  // a subset.
  static SkBitmap Resize(const SkBitmap& source,
                         ResizeMethod method,
                         int dest_width, int dest_height);

 private:
  ImageOperations();  // Class for scoping only.

  // Supports all methods except RESIZE_SUBPIXEL.
  static SkBitmap ResizeBasic(const SkBitmap& source,
                              ResizeMethod method,
                              int dest_width, int dest_height,
                              const SkIRect& dest_subset);

  // Subpixel renderer.
  static SkBitmap ResizeSubpixel(const SkBitmap& source,
                                 int dest_width, int dest_height,
                                 const SkIRect& dest_subset);
};

}  // namespace skia

#endif  // SKIA_EXT_IMAGE_OPERATIONS_H_
