// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_rep.h"

namespace gfx {

ImageSkiaRep::ImageSkiaRep()
    : scale_factor_(ui::SCALE_FACTOR_NONE) {
}

ImageSkiaRep::~ImageSkiaRep() {
}

ImageSkiaRep::ImageSkiaRep(int width, int height,
                           ui::ScaleFactor scale_factor)
    : scale_factor_(scale_factor) {
  float scale = ui::GetScaleFactorScale(scale_factor);
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
                    static_cast<int>(width * scale),
                    static_cast<int>(height * scale));
  bitmap_.allocPixels();
}

ImageSkiaRep::ImageSkiaRep(const SkBitmap& src)
    : bitmap_(src),
      scale_factor_(ui::SCALE_FACTOR_NONE) {
}

ImageSkiaRep::ImageSkiaRep(const SkBitmap& src,
                           ui::ScaleFactor scale_factor)
    : bitmap_(src),
      scale_factor_(scale_factor) {
}

ImageSkiaRep& ImageSkiaRep::operator=(const SkBitmap& other) {
  bitmap_ = other;
  scale_factor_ = ui::SCALE_FACTOR_NONE;
  return *this;
}

ImageSkiaRep::operator SkBitmap&() const {
  return const_cast<SkBitmap&>(bitmap_);
}

int ImageSkiaRep::GetWidth() const {
  return static_cast<int>(bitmap_.width() /
      ui::GetScaleFactorScale(scale_factor_));
}

int ImageSkiaRep::GetHeight() const {
  return static_cast<int>(bitmap_.height() /
      ui::GetScaleFactorScale(scale_factor_));
}

float ImageSkiaRep::GetScale() const {
  return ui::GetScaleFactorScale(scale_factor_);
}

}  // namespace gfx
