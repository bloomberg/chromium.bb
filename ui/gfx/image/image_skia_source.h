// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_SOURCE_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_SOURCE_H_
#pragma once

#include "ui/base/layout.h"

namespace gfx {

class ImageSkiaRep;

class ImageSkiaSource {
 public:
  virtual ~ImageSkiaSource() {}

  // Returns the ImageSkiaRep for the given |scale_factor|.
  virtual gfx::ImageSkiaRep GetImageForScale(ui::ScaleFactor scale_factor) = 0;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_SOURCE_H_
