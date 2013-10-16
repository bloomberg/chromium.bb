// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/rect_based_targeting_utils.h"

#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace views {

namespace {

// The positive distance from |pos| to the nearest endpoint of the interval
// [start, end] is returned if |pos| lies within the interval, otherwise
// 0 is returned.
int DistanceToInterval(int pos, int start, int end) {
  if (pos < start)
    return start - pos;
  if (pos > end)
    return pos - end;
  return 0;
}

}  // namespace

bool UsePointBasedTargeting(const gfx::Rect& rect) {
  return rect.width() == 1 && rect.height() == 1;
}

float PercentCoveredBy(const gfx::Rect& rect_1, const gfx::Rect& rect_2) {
  gfx::Rect intersection(rect_1);
  intersection.Intersect(rect_2);
  int intersect_area = intersection.size().GetArea();
  int rect_1_area = rect_1.size().GetArea();
  return rect_1_area ?
      static_cast<float>(intersect_area) / static_cast<float>(rect_1_area) : 0;
}

int DistanceSquaredFromCenterLineToPoint(const gfx::Point& point,
                                         const gfx::Rect& rect) {
  gfx::Point center_point = rect.CenterPoint();
  int dx = center_point.x() - point.x();
  int dy = center_point.y() - point.y();

  if (rect.width() > rect.height()) {
    dx = DistanceToInterval(point.x(),
                            rect.x() + (rect.height() / 2),
                            rect.right() - (rect.height() / 2));
  } else {
    dy = DistanceToInterval(point.y(),
                            rect.y() + (rect.width() / 2),
                            rect.bottom() - (rect.width() / 2));
  }
  return (dx * dx) + (dy * dy);
}

}  // namespace views
