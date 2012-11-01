// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rect_f.h"

#include <algorithm>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "ui/gfx/insets_f.h"
#include "ui/gfx/rect_base_impl.h"

namespace gfx {

template class RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF, float>;

typedef class RectBase<RectF, PointF, SizeF, InsetsF, Vector2dF,
                       float> RectBaseT;

RectF::RectF() : RectBaseT(gfx::SizeF()) {
}

RectF::RectF(float width, float height)
    : RectBaseT(gfx::SizeF(width, height)) {
}

RectF::RectF(float x, float y, float width, float height)
    : RectBaseT(gfx::PointF(x, y), gfx::SizeF(width, height)) {
}

RectF::RectF(const gfx::SizeF& size)
    : RectBaseT(size) {
}

RectF::RectF(const gfx::PointF& origin, const gfx::SizeF& size)
    : RectBaseT(origin, size) {
}

RectF::~RectF() {}

std::string RectF::ToString() const {
  return base::StringPrintf("%s %s",
                            origin().ToString().c_str(),
                            size().ToString().c_str());
}

RectF IntersectRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Intersect(b);
  return result;
}

RectF UnionRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Union(b);
  return result;
}

RectF SubtractRects(const RectF& a, const RectF& b) {
  RectF result = a;
  result.Subtract(b);
  return result;
}

RectF ScaleRect(const RectF& r, float x_scale, float y_scale) {
  RectF result = r;
  result.Scale(x_scale, y_scale);
  return result;
}

RectF BoundingRect(const PointF& p1, const PointF& p2) {
  float rx = std::min(p1.x(), p2.x());
  float ry = std::min(p1.y(), p2.y());
  float rr = std::max(p1.x(), p2.x());
  float rb = std::max(p1.y(), p2.y());
  return RectF(rx, ry, rr - rx, rb - ry);
}

}  // namespace gfx
