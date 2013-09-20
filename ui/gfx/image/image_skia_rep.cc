// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_rep.h"

namespace gfx {

ImageSkiaRep::ImageSkiaRep() : scale_(1.0f) {
}

ImageSkiaRep::~ImageSkiaRep() {
}

ImageSkiaRep::ImageSkiaRep(const gfx::Size& size, float scale) : scale_(scale) {
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
                    static_cast<int>(size.width() * scale),
                    static_cast<int>(size.height() * scale));
  bitmap_.allocPixels();
}

ImageSkiaRep::ImageSkiaRep(const SkBitmap& src, float scale)
    : bitmap_(src),
      scale_(scale) {
}

int ImageSkiaRep::GetWidth() const {
  return static_cast<int>(bitmap_.width() / scale_);
}

int ImageSkiaRep::GetHeight() const {
  return static_cast<int>(bitmap_.height() / scale_);
}

}  // namespace gfx
