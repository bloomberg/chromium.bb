// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point_f.h"

#include <cmath>

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"

#if defined(OS_IOS)
#include <CoreGraphics/CoreGraphics.h>
#elif defined(OS_MAC)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

#if defined(OS_APPLE)
PointF::PointF(const CGPoint& p) : PointF(p.x, p.y) {}
CGPoint PointF::ToCGPoint() const {
  return CGPointMake(x(), y());
}
#endif

void PointF::SetToMin(const PointF& other) {
  x_ = std::min(x_, other.x_);
  y_ = std::min(y_, other.y_);
}

void PointF::SetToMax(const PointF& other) {
  x_ = std::max(x_, other.x_);
  y_ = std::max(y_, other.y_);
}

bool PointF::IsWithinDistance(const PointF& rhs,
                              const float allowed_distance) const {
  DCHECK(allowed_distance > 0);
  float diff_x = x_ - rhs.x();
  float diff_y = y_ - rhs.y();
  float distance = std::sqrt(diff_x * diff_x + diff_y * diff_y);
  return distance < allowed_distance;
}

std::string PointF::ToString() const {
  return base::StringPrintf("%g,%g", x(), y());
}

PointF ScalePoint(const PointF& p, float x_scale, float y_scale) {
  PointF scaled_p(p);
  scaled_p.Scale(x_scale, y_scale);
  return scaled_p;
}


}  // namespace gfx
