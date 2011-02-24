// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#ifndef UI_GFX_IMAGE_UNITTEST_H_
#define UI_GFX_IMAGE_UNITTEST_H_

#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

#if defined(OS_LINUX)
#include "ui/gfx/gtk_util.h"
#elif defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "skia/ext/skia_utils_mac.h"
#endif

namespace gfx {
namespace test {

#if defined(OS_MACOSX)
typedef NSImage* PlatformImage;
#elif defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
typedef GdkPixbuf* PlatformImage;
#else
typedef const SkBitmap* PlatformImage;
#endif

SkBitmap* CreateBitmap() {
  SkBitmap* bitmap = new SkBitmap();
  bitmap->setConfig(SkBitmap::kARGB_8888_Config, 25, 25);
  bitmap->allocPixels();
  bitmap->eraseRGB(255, 0, 0);
  return bitmap;
}

PlatformImage CreatePlatformImage() {
  scoped_ptr<SkBitmap> bitmap(CreateBitmap());
#if defined(OS_MACOSX)
  NSImage* image = gfx::SkBitmapToNSImage(*(bitmap.get()));
  base::mac::NSObjectRetain(image);
  return image;
#elif defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  return gfx::GdkPixbufFromSkBitmap(bitmap.get());
#else
  return bitmap.release();
#endif
}

gfx::Image::RepresentationType GetPlatformRepresentationType() {
#if defined(OS_MACOSX)
  return gfx::Image::kNSImageRep;
#elif defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  return gfx::Image::kGdkPixbufRep;
#else
  return gfx::Image::kSkBitmapRep;
#endif
}

}  // namespace test
}  // namespace gfx

#endif  // UI_GFX_IMAGE_UNITTEST_H_
