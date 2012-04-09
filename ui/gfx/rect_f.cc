// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rect_f.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "ui/gfx/insets_f.h"

namespace {

void AdjustAlongAxis(float dst_origin,
                     float dst_size,
                     float* origin,
                     float* size) {
  *size = std::min(dst_size, *size);
  if (*origin < dst_origin)
    *origin = dst_origin;
  else
    *origin = std::min(dst_origin + dst_size, *origin + *size) - *size;
}

} // namespace

namespace gfx {

RectF::RectF() {
}

RectF::RectF(float width, float height)
    : size_(width, height) {
}

RectF::RectF(float x, float y, float width, float height)
    : origin_(x, y), size_(width, height) {
}

RectF::RectF(const gfx::SizeF& size)
    : size_(size) {
}

RectF::RectF(const gfx::PointF& origin, const gfx::SizeF& size)
    : origin_(origin), size_(size) {
}

RectF::~RectF() {}

void RectF::SetRect(float x, float y, float width, float height) {
  origin_.SetPoint(x, y);
  set_width(width);
  set_height(height);
}

void RectF::Inset(const gfx::InsetsF& insets) {
  Inset(insets.left(), insets.top(), insets.right(), insets.bottom());
}

void RectF::Inset(float left, float top, float right, float bottom) {
  Offset(left, top);
  set_width(std::max(width() - left - right, 0.0f));
  set_height(std::max(height() - top - bottom, 0.0f));
}

void RectF::Offset(float horizontal, float vertical) {
  origin_.Offset(horizontal, vertical);
}

bool RectF::operator==(const RectF& other) const {
  return origin_ == other.origin_ && size_ == other.size_;
}

bool RectF::operator<(const RectF& other) const {
  if (origin_ == other.origin_) {
    if (width() == other.width()) {
      return height() < other.height();
    } else {
      return width() < other.width();
    }
  } else {
    return origin_ < other.origin_;
  }
}

bool RectF::Contains(float point_x, float point_y) const {
  return (point_x >= x()) && (point_x < right()) &&
         (point_y >= y()) && (point_y < bottom());
}

bool RectF::Contains(const RectF& rect) const {
  return (rect.x() >= x() && rect.right() <= right() &&
          rect.y() >= y() && rect.bottom() <= bottom());
}

bool RectF::Intersects(const RectF& rect) const {
  return !(rect.x() >= right() || rect.right() <= x() ||
           rect.y() >= bottom() || rect.bottom() <= y());
}

RectF RectF::Intersect(const RectF& rect) const {
  float rx = std::max(x(), rect.x());
  float ry = std::max(y(), rect.y());
  float rr = std::min(right(), rect.right());
  float rb = std::min(bottom(), rect.bottom());

  if (rx >= rr || ry >= rb)
    rx = ry = rr = rb = 0;  // non-intersecting

  return RectF(rx, ry, rr - rx, rb - ry);
}

RectF RectF::Union(const RectF& rect) const {
  // special case empty rects...
  if (IsEmpty())
    return rect;
  if (rect.IsEmpty())
    return *this;

  float rx = std::min(x(), rect.x());
  float ry = std::min(y(), rect.y());
  float rr = std::max(right(), rect.right());
  float rb = std::max(bottom(), rect.bottom());

  return RectF(rx, ry, rr - rx, rb - ry);
}

RectF RectF::Subtract(const RectF& rect) const {
  // boundary cases:
  if (!Intersects(rect))
    return *this;
  if (rect.Contains(*this))
    return RectF();

  float rx = x();
  float ry = y();
  float rr = right();
  float rb = bottom();

  if (rect.y() <= y() && rect.bottom() >= bottom()) {
    // complete intersection in the y-direction
    if (rect.x() <= x()) {
      rx = rect.right();
    } else {
      rr = rect.x();
    }
  } else if (rect.x() <= x() && rect.right() >= right()) {
    // complete intersection in the x-direction
    if (rect.y() <= y()) {
      ry = rect.bottom();
    } else {
      rb = rect.y();
    }
  }
  return RectF(rx, ry, rr - rx, rb - ry);
}

RectF RectF::AdjustToFit(const RectF& rect) const {
  float new_x = x();
  float new_y = y();
  float new_width = width();
  float new_height = height();
  AdjustAlongAxis(rect.x(), rect.width(), &new_x, &new_width);
  AdjustAlongAxis(rect.y(), rect.height(), &new_y, &new_height);
  return RectF(new_x, new_y, new_width, new_height);
}

PointF RectF::CenterPoint() const {
  return PointF(x() + (width() - 1) / 2, y() + (height() - 1) / 2);
}

RectF RectF::Center(const gfx::SizeF& size) const {
  float new_width = std::min(width(), size.width());
  float new_height = std::min(height(), size.height());
  float new_x = x() + (width() - new_width) / 2;
  float new_y = y() + (height() - new_height) / 2;
  return RectF(new_x, new_y, new_width, new_height);
}

void RectF::SplitVertically(gfx::RectF* left_half,
                            gfx::RectF* right_half) const {
  DCHECK(left_half);
  DCHECK(right_half);

  left_half->SetRect(this->x(), this->y(), this->width() / 2, this->height());
  right_half->SetRect(left_half->right(),
                      this->y(),
                      this->width() - left_half->width(),
                      this->height());
}

bool RectF::SharesEdgeWith(const gfx::RectF& rect) const {
  return (y() == rect.y() && height() == rect.height() &&
             (x() == rect.right() || right() == rect.x())) ||
         (x() == rect.x() && width() == rect.width() &&
             (y() == rect.bottom() || bottom() == rect.y()));
}

std::string RectF::ToString() const {
  return base::StringPrintf("%s %s",
                            origin_.ToString().c_str(),
                            size_.ToString().c_str());
}

}  // namespace gfx
