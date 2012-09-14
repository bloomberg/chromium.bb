// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Because the unit tests for gfx::Image are spread across multiple
// implementation files, this header contains the reusable components.

#ifndef UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_
#define UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_

#include "ui/gfx/image/image.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
namespace test {

#if defined(OS_IOS)
typedef UIImage* PlatformImage;
#elif defined(OS_MACOSX)
typedef NSImage* PlatformImage;
#elif defined(TOOLKIT_GTK)
typedef GdkPixbuf* PlatformImage;
#else
typedef const SkBitmap PlatformImage;
#endif

void SetSupportedScaleFactorsTo1xAnd2x();

const SkBitmap CreateBitmap(int width, int height);

// TODO(rohitrao): Remove the no-argument version of CreateImage().
gfx::Image CreateImage();
gfx::Image CreateImage(int width, int height);

bool IsEqual(const gfx::Image& image1, const gfx::Image& image2);

bool IsEmpty(const gfx::Image& image);

PlatformImage CreatePlatformImage();

gfx::Image::RepresentationType GetPlatformRepresentationType();

PlatformImage ToPlatformType(const gfx::Image& image);
PlatformImage CopyPlatformType(const gfx::Image& image);

SkColor GetPlatformImageColor(PlatformImage image);
void CheckColor(SkColor color, bool is_red);

bool IsPlatformImageValid(PlatformImage image);

bool PlatformImagesEqual(PlatformImage image1, PlatformImage image2);

}  // namespace test
}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_UNITTEST_UTIL_H_
