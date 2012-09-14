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

  // iOS only supports one scale factor.
  std::vector<ui::ScaleFactor> supported_scale_factors =
      ui::GetSupportedScaleFactors();
  DCHECK_EQ(1U, supported_scale_factors.size());
  if (supported_scale_factors.size() < 1)
    return image_skia;

  ui::ScaleFactor scale_factor = supported_scale_factors[0];
  float scale = ui::GetScaleFactorScale(scale_factor);
  CGSize size = image.size;
  CGSize desired_size_for_scale =
      CGSizeMake(size.width * scale, size.height * scale);
  SkBitmap bitmap(gfx::UIImageToSkBitmap(image, desired_size_for_scale, false));
  if (!bitmap.isNull())
    image_skia.AddRepresentation(gfx::ImageSkiaRep(bitmap, scale_factor));
  return image_skia;
}

UIImage* UIImageFromImageSkia(const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull())
    return nil;

  // iOS only supports one scale factor.
  std::vector<ui::ScaleFactor> supported_scale_factors =
      ui::GetSupportedScaleFactors();
  DCHECK_EQ(1U, supported_scale_factors.size());
  if (supported_scale_factors.size() < 1)
    return nil;

  image_skia.EnsureRepsForSupportedScaleFactors();
  const ImageSkiaRep& rep =
      image_skia.GetRepresentation(supported_scale_factors[0]);
  base::mac::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  return gfx::SkBitmapToUIImageWithColorSpace(rep.sk_bitmap(), color_space);
}

}  // namespace gfx
