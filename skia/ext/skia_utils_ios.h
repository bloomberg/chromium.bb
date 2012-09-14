// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SKIA_EXT_SKIA_UTILS_IOS_H_
#define SKIA_EXT_SKIA_UTILS_IOS_H_

#include <CoreGraphics/CoreGraphics.h>
#include <vector>

#include "third_party/skia/include/core/SkBitmap.h"

#ifdef __OBJC__
@class UIImage;
#else
class UIImage;
#endif

namespace gfx {

// Draws a UIImage with a given size into a SkBitmap.
SK_API SkBitmap UIImageToSkBitmap(UIImage* image, CGSize size, bool is_opaque);

// Given an SkBitmap and a color space, return an autoreleased UIImage.
SK_API UIImage* SkBitmapToUIImageWithColorSpace(const SkBitmap& skia_bitmap,
                                                CGColorSpaceRef color_space);

}  // namespace gfx

#endif  // SKIA_EXT_SKIA_UTILS_IOS_H_
