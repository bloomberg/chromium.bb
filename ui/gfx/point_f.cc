// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/point_f.h"

#include "base/stringprintf.h"
#include "ui/gfx/point.h"

namespace gfx {

PointF::PointF() : x_(0), y_(0) {
}

PointF::PointF(float x, float y) : x_(x), y_(y) {
}

Point PointF::ToPoint() const {
  return Point(static_cast<int>(x_), static_cast<int>(y_));
}

std::string PointF::ToString() const {
  return base::StringPrintf("%f,%f", x_, y_);
}

}  // namespace gfx
