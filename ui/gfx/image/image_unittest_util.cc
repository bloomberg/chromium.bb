// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(TOOLKIT_USES_GTK)
#include "ui/gfx/gtk_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace gfx {
namespace test {

SkBitmap* CreateBitmap(int width, int height) {
  SkBitmap* bitmap = new SkBitmap();
  bitmap->setConfig(SkBitmap::kARGB_8888_Config, width, height);
  bitmap->allocPixels();
  bitmap->eraseRGB(255, 0, 0);
  return bitmap;
}

gfx::Image CreateImage() {
  return gfx::Image(CreateBitmap(100, 50));
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
  scoped_ptr<SkBitmap> bitmap(CreateBitmap(25, 25));
#if defined(OS_MACOSX)
  NSImage* image = gfx::SkBitmapToNSImage(*(bitmap.get()));
  base::mac::NSObjectRetain(image);
  return image;
#elif defined(TOOLKIT_GTK)
  return gfx::GdkPixbufFromSkBitmap(bitmap.get());
#else
  return bitmap.release();
#endif
}

gfx::Image::RepresentationType GetPlatformRepresentationType() {
#if defined(OS_MACOSX)
  return gfx::Image::kImageRepCocoa;
#elif defined(TOOLKIT_GTK)
  return gfx::Image::kImageRepGdk;
#else
  return gfx::Image::kImageRepSkia;
#endif
}

PlatformImage ToPlatformType(const gfx::Image& image) {
#if defined(OS_MACOSX)
  return image.ToNSImage();
#elif defined(TOOLKIT_GTK)
  return image.ToGdkPixbuf();
#else
  return image.ToSkBitmap();
#endif
}

}  // namespace test
}  // namespace gfx
