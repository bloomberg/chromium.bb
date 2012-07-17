// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_OPERATIONS_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_OPERATIONS_H_

#include "base/gtest_prod_util.h"
#include "ui/base/ui_export.h"
#include "ui/gfx/shadow_value.h"

namespace gfx {
class ImageSkia;
class Size;

class UI_EXPORT ImageSkiaOperations {
 public:
  // Create an image that is a blend of two others. The alpha argument
  // specifies the opacity of the second imag. The provided image must
  // use the kARGB_8888_Config config and be of equal dimensions.
  static ImageSkia CreateBlendedImage(const ImageSkia& first,
                                      const ImageSkia& second,
                                      double alpha);

  // Create an image that is the original image masked out by the mask defined
  // in the alpha image. The images must use the kARGB_8888_Config config and
  // be of equal dimensions.
  static ImageSkia CreateMaskedImage(const ImageSkia& first,
                                     const ImageSkia& alpha);

  // Create an image that is cropped from another image. This is special
  // because it tiles the original image, so your coordinates can extend
  // outside the bounds of the original image.
  static ImageSkia CreateTiledImage(const ImageSkia& image,
                                    int src_x, int src_y,
                                    int dst_w, int dst_h);

  // Creates an image by resizing |source| to given |target_dip_size|.
  static ImageSkia CreateResizedImage(const ImageSkia& source,
                                      const Size& target_dip_size);

  // Creates an image with drop shadow defined in |shadows| for |source|.
  static ImageSkia CreateImageWithDropShadow(const ImageSkia& source,
                                             const ShadowValues& shadows);

 private:
  ImageSkiaOperations();  // Class for scoping only.
};

}

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_OPERATIONS_H_
