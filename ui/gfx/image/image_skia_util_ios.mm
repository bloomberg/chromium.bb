// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_util_ios.h"

#include <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "skia/ext/skia_utils_ios.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {

gfx::ImageSkia ImageSkiaFromUIImage(UIImage* image) {
  gfx::ImageSkia image_skia;
  if (!image)
    return image_skia;

  ui::ScaleFactor scale_factor = ui::GetMaxScaleFactor();
  float scale = ui::GetScaleFactorScale(scale_factor);
  CGSize size = image.size;
  CGSize desired_size_for_scale =
      CGSizeMake(size.width * scale, size.height * scale);
  SkBitmap bitmap(gfx::CGImageToSkBitmap(image.CGImage,
                                         desired_size_for_scale,
                                         false));
  if (!bitmap.isNull())
    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale_factor));
  return image_skia;
}

UIImage* UIImageFromImageSkia(const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull())
    return nil;

  ui::ScaleFactor scale_factor = ui::GetMaxScaleFactor();
  float scale = ui::GetScaleFactorScale(scale_factor);
  image_skia.EnsureRepsForSupportedScaleFactors();
  const ImageSkiaRep& rep = image_skia.GetRepresentation(scale_factor);
  base::mac::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  return gfx::SkBitmapToUIImageWithColorSpace(rep.sk_bitmap(), scale,
                                              color_space);
}

}  // namespace gfx
