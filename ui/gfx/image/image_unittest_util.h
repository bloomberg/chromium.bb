// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#ifndef UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_
#define UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_

#include "ui/gfx/image/image.h"

namespace gfx {
namespace test {

#if defined(OS_MACOSX)
typedef NSImage* PlatformImage;
#elif defined(TOOLKIT_GTK)
typedef GdkPixbuf* PlatformImage;
#else
typedef const SkBitmap* PlatformImage;
#endif

SkBitmap* CreateBitmap(int width, int height);

PlatformImage CreatePlatformImage();

gfx::Image::RepresentationType GetPlatformRepresentationType();

PlatformImage ToPlatformType(const gfx::Image& image);

}  // namespace test
}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_
