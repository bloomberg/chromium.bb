// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_UTIL_IOS_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_UTIL_IOS_H_

#include "ui/base/ui_export.h"

#ifdef __OBJC__
@class UIImage;
#else
class UIImage;
#endif

namespace gfx {
class ImageSkia;

// Converts to ImageSkia from UIImage.
UI_EXPORT gfx::ImageSkia ImageSkiaFromUIImage(UIImage* image);

// Converts to UIImage from ImageSkia.  Returns an autoreleased UIImage.
UI_EXPORT UIImage* UIImageFromImageSkia(const gfx::ImageSkia& image_skia);

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_UTIL_IOS_H_
