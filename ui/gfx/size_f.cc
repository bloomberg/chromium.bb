// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/size_f.h"

#include "base/logging.h"
#include "base/stringprintf.h"

namespace gfx {

template class SizeBase<SizeF, float>;

SizeF::SizeF() : SizeBase<SizeF, float>(0, 0) {}

SizeF::SizeF(float width, float height) : SizeBase<SizeF, float>(0, 0) {
  set_width(width);
  set_height(height);
}

SizeF::~SizeF() {}

std::string SizeF::ToString() const {
  return base::StringPrintf("%fx%f", width(), height());
}

SizeF ScaleSize(const SizeF& s, float x_scale, float y_scale) {
  SizeF scaled_s(s);
  scaled_s.Scale(x_scale, y_scale);
  return scaled_s;
}

}  // namespace gfx
