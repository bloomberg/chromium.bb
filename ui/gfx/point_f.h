// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT_F_H_
#define UI_GFX_POINT_F_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/base/ui_export.h"

#if !defined(ENABLE_DIP)
#error "This class should be used only when DIP feature is enabled"
#endif

namespace gfx {
class Point;

// A floating versino of gfx::Point. This is copied, instead of using
// template, to avoid conflict with m19 branch.
// TODO(oshima): Merge this to ui/gfx/point.h using template.
class UI_EXPORT PointF {
 public:
  PointF();
  PointF(float x, float y);
  PointF(Point& point);
  ~PointF() {}

  float x() const { return x_; }
  float y() const { return y_; }

  void SetPoint(float x, float y) {
    x_ = x;
    y_ = y;
  }

  void set_x(float x) { x_ = x; }
  void set_y(float y) { y_ = y; }

  void Offset(float delta_x, float delta_y) {
    x_ += delta_x;
    y_ += delta_y;
  }

  PointF Add(const PointF& other) const{
    PointF copy = *this;
    copy.Offset(other.x_, other.y_);
    return copy;
  }

  PointF Subtract(const PointF& other) const {
    PointF copy = *this;
    copy.Offset(-other.x_, -other.y_);
    return copy;
  }

  PointF Middle(const PointF& other) const {
    return PointF((x_ + other.x_) / 2, (y_ + other.y_) / 2);
  }

  bool operator==(const PointF& rhs) const {
    return x_ == rhs.x_ && y_ == rhs.y_;
  }

  bool operator!=(const PointF& rhs) const {
    return !(*this == rhs);
  }

  // A point is less than another point if its y-value is closer
  // to the origin. If the y-values are the same, then point with
  // the x-value closer to the origin is considered less than the
  // other.
  // This comparison is required to use Points in sets, or sorted
  // vectors.
  bool operator<(const PointF& rhs) const {
    return (y_ == rhs.y_) ? (x_ < rhs.x_) : (y_ < rhs.y_);
  }

  Point ToPoint() const;

  // Returns a string representation of point.
  std::string ToString() const;

 private:
  float x_;
  float y_;
};

}  // namespace gfx

#endif  // UI_GFX_POINT_F_H_
