// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/size_f.h"

#include "base/logging.h"
#include "base/stringprintf.h"

namespace gfx {

SizeF::SizeF() : width_(0), height_(0) {}

SizeF::SizeF(float width, float height) {
  set_width(width);
  set_height(height);
}

SizeF::~SizeF() {}

void SizeF::set_width(float width) {
  if (width < 0) {
    NOTREACHED() << "negative width:" << width;
    width = 0;
  }
  width_ = width;
}

void SizeF::set_height(float height) {
  if (height < 0) {
    NOTREACHED() << "negative height:" << height;
    height = 0;
  }
  height_ = height;
}

std::string SizeF::ToString() const {
  return base::StringPrintf("%fx%f", width_, height_);
}

}  // namespace gfx
