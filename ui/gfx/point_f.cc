// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/point_f.h"

#include <cmath>

#include "base/stringprintf.h"
#include "ui/gfx/point.h"

namespace gfx {

template class PointBase<PointF, float>;

PointF::PointF() : PointBase<PointF, float>(0, 0) {
}

PointF::PointF(float x, float y) : PointBase<PointF, float>(x, y) {
}

Point PointF::ToPoint() const {
  return Point(static_cast<int>(std::floor(x())),
               static_cast<int>(std::floor(y())));
}

std::string PointF::ToString() const {
  return base::StringPrintf("%f,%f", x(), y());
}

}  // namespace gfx
