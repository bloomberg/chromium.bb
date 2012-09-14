// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#include "base/memory/scoped_ptr.h"
#include "ui/base/layout.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#include "ui/gfx/gtk_util.h"
#elif defined(OS_IOS)
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "skia/ext/skia_utils_ios.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace gfx {
namespace test {

void SetSupportedScaleFactorsTo1xAnd2x() {
  std::vector<ui::ScaleFactor> supported_scale_factors;
  supported_scale_factors.push_back(ui::SCALE_FACTOR_100P);
  supported_scale_factors.push_back(ui::SCALE_FACTOR_200P);
  ui::test::SetSupportedScaleFactors(supported_scale_factors);
}

const SkBitmap CreateBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap.allocPixels();
  bitmap.eraseRGB(0, 255, 0);
  return bitmap;
}

gfx::Image CreateImage() {
  return CreateImage(100, 50);
}

gfx::Image CreateImage(int width, int height) {
  return gfx::Image(CreateBitmap(width, height));
}

bool IsEqual(const gfx::Image& image1, const gfx::Image& image2) {
  const SkBitmap& bmp1 = *image1.ToSkBitmap();
  const SkBitmap& bmp2 = *image2.ToSkBitmap();

  if (bmp1.width() != bmp2.width() ||
      bmp1.height() != bmp2.height() ||
      bmp1.config() != SkBitmap::kARGB_8888_Config ||
      bmp2.config() != SkBitmap::kARGB_8888_Config) {
    return false;
  }

  SkAutoLockPixels lock1(bmp1);
  SkAutoLockPixels lock2(bmp2);
  if (!bmp1.getPixels() || !bmp2.getPixels())
    return false;

  for (int y = 0; y < bmp1.height(); ++y) {
    for (int x = 0; x < bmp1.width(); ++x) {
      if (*bmp1.getAddr32(x,y) != *bmp2.getAddr32(x,y))
        return false;
    }
  }

  return true;
}

bool IsEmpty(const gfx::Image& image) {
  const SkBitmap& bmp = *image.ToSkBitmap();
  return bmp.isNull() ||
         (bmp.width() == 0 && bmp.height() == 0);
}

PlatformImage CreatePlatformImage() {
  const SkBitmap bitmap(CreateBitmap(25, 25));
#if defined(OS_IOS)
  base::mac::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  UIImage* image = gfx::SkBitmapToUIImageWithColorSpace(bitmap, color_space);
  base::mac::NSObjectRetain(image);
  return image;
#elif defined(OS_MACOSX)
  NSImage* image = gfx::SkBitmapToNSImage(bitmap);
  base::mac::NSObjectRetain(image);
  return image;
#elif defined(TOOLKIT_GTK)
  return gfx::GdkPixbufFromSkBitmap(bitmap);
#else
  return bitmap;
#endif
}

gfx::Image::RepresentationType GetPlatformRepresentationType() {
#if defined(OS_IOS)
  return gfx::Image::kImageRepCocoaTouch;
#elif defined(OS_MACOSX)
  return gfx::Image::kImageRepCocoa;
#elif defined(TOOLKIT_GTK)
  return gfx::Image::kImageRepGdk;
#else
  return gfx::Image::kImageRepSkia;
#endif
}

PlatformImage ToPlatformType(const gfx::Image& image) {
#if defined(OS_IOS)
  return image.ToUIImage();
#elif defined(OS_MACOSX)
  return image.ToNSImage();
#elif defined(TOOLKIT_GTK)
  return image.ToGdkPixbuf();
#else
  return *image.ToSkBitmap();
#endif
}

PlatformImage CopyPlatformType(const gfx::Image& image) {
#if defined(OS_IOS)
  return image.CopyUIImage();
#elif defined(OS_MACOSX)
  return image.CopyNSImage();
#elif defined(TOOLKIT_GTK)
  return image.CopyGdkPixbuf();
#else
  return *image.ToSkBitmap();
#endif
}

#if defined(OS_MACOSX)
// Defined in image_unittest_util_mac.mm.
#elif defined(TOOLKIT_GTK)
SkColor GetPlatformImageColor(PlatformImage image) {
  guchar* gdk_pixels = gdk_pixbuf_get_pixels(image);
  guchar alpha = gdk_pixbuf_get_has_alpha(image) ? gdk_pixels[3] : 255;
  return SkColorSetARGB(alpha, gdk_pixels[0], gdk_pixels[1], gdk_pixels[2]);
}
#else
SkColor GetPlatformImageColor(PlatformImage image) {
  SkAutoLockPixels auto_lock(image);
  return image.getColor(10, 10);
}
#endif

void CheckColor(SkColor color, bool is_red) {
  // Be tolerant of floating point rounding and lossy color space conversions.
  if (is_red) {
    EXPECT_GT(SkColorGetR(color), 0.95);
    EXPECT_LT(SkColorGetG(color), 0.05);
  } else {
    EXPECT_GT(SkColorGetG(color), 0.95);
    EXPECT_LT(SkColorGetR(color), 0.05);
  }
  EXPECT_LT(SkColorGetB(color), 0.05);
  EXPECT_GT(SkColorGetA(color), 0.95);
}

bool IsPlatformImageValid(PlatformImage image) {
#if defined(OS_MACOSX) || defined(TOOLKIT_GTK)
  return image != NULL;
#else
  return !image.isNull();
#endif
}

bool PlatformImagesEqual(PlatformImage image1, PlatformImage image2) {
#if defined(OS_MACOSX) || defined(TOOLKIT_GTK)
  return image1 == image2;
#else
  return image1.getPixels() == image2.getPixels();
#endif
}

}  // namespace test
}  // namespace gfx
