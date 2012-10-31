// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/point_f.h"

#include "base/stringprintf.h"

namespace gfx {

template class PointBase<PointF, float, Vector2dF>;

PointF::PointF() : PointBase<PointF, float, Vector2dF>(0, 0) {
}

PointF::PointF(float x, float y) : PointBase<PointF, float, Vector2dF>(x, y) {
}

PointF::~PointF() {}

std::string PointF::ToString() const {
  return base::StringPrintf("%f,%f", x(), y());
}

}  // namespace gfx
