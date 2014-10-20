// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/point.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/strings/stringprintf.h"

namespace gfx {

#if defined(OS_WIN)
Point::Point(DWORD point) {
  POINTS points = MAKEPOINTS(point);
  x_ = points.x;
  y_ = points.y;
}

Point::Point(const POINT& point) : x_(point.x), y_(point.y) {
}

Point& Point::operator=(const POINT& point) {
  x_ = point.x;
  y_ = point.y;
  return *this;
}

POINT Point::ToPOINT() const {
  POINT p;
  p.x = x();
  p.y = y();
  return p;
}
#endif

void Point::SetToMin(const Point& other) {
  x_ = x_ <= other.x_ ? x_ : other.x_;
  y_ = y_ <= other.y_ ? y_ : other.y_;
}

void Point::SetToMax(const Point& other) {
  x_ = x_ >= other.x_ ? x_ : other.x_;
  y_ = y_ >= other.y_ ? y_ : other.y_;
}

std::string Point::ToString() const {
  return base::StringPrintf("%d,%d", x(), y());
}

}  // namespace gfx
