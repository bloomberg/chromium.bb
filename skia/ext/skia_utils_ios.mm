// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/skia_utils_ios.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

namespace gfx {

SkBitmap UIImageToSkBitmap(UIImage* image, CGSize size, bool is_opaque) {
  SkBitmap bitmap;
  if (!image)
    return bitmap;

  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width, size.height);
  if (!bitmap.allocPixels())
    return bitmap;

  bitmap.setIsOpaque(is_opaque);
  void* data = bitmap.getPixels();

  // Allocate a bitmap context with 4 components per pixel (BGRA). Apple
  // recommends these flags for improved CG performance.
#define HAS_ARGB_SHIFTS(a, r, g, b) \
            (SK_A32_SHIFT == (a) && SK_R32_SHIFT == (r) \
             && SK_G32_SHIFT == (g) && SK_B32_SHIFT == (b))
#if defined(SK_CPU_LENDIAN) && HAS_ARGB_SHIFTS(24, 16, 8, 0)
  base::mac::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::mac::ScopedCFTypeRef<CGContextRef> context(
      CGBitmapContextCreate(data, size.width, size.height, 8, size.width*4,
                            color_space,
                            kCGImageAlphaPremultipliedFirst |
                                kCGBitmapByteOrder32Host));
#else
#error We require that Skia's and CoreGraphics's recommended \
       image memory layout match.
#endif
#undef HAS_ARGB_SHIFTS

  DCHECK(context);
  if (!context)
    return bitmap;

  // UIGraphicsPushContext be called from the main thread.
  // TODO(rohitrao): We can use CG to make this thread safe, but the mac code
  // calls setCurrentContext, so it's similarly limited to the main thread.
  DCHECK([NSThread isMainThread]);
  UIGraphicsPushContext(context);
  [image drawInRect:CGRectMake(0, 0, size.width, size.height)
          blendMode:kCGBlendModeCopy
              alpha:1.0];
  UIGraphicsPopContext();

  return bitmap;
}

UIImage* SkBitmapToUIImageWithColorSpace(const SkBitmap& skia_bitmap,
                                         CGColorSpaceRef color_space) {
  if (skia_bitmap.isNull())
    return nil;

  // First convert SkBitmap to CGImageRef.
  base::mac::ScopedCFTypeRef<CGImageRef> cg_image(
      SkCreateCGImageRefWithColorspace(skia_bitmap, color_space));

  // Now convert to UIImage.
  // TODO(rohitrao): Gotta incorporate the scale factor somewhere!
  return [UIImage imageWithCGImage:cg_image.get()];
}

}  // namespace gfx
