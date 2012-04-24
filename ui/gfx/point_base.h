// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT_BASE_H_
#define UI_GFX_POINT_BASE_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/base/ui_export.h"

namespace gfx {

// A point has an x and y coordinate.
template<typename Class, typename Type>
class UI_EXPORT PointBase {
 public:
  Type x() const { return x_; }
  Type y() const { return y_; }

  void SetPoint(Type x, Type y) {
    x_ = x;
    y_ = y;
  }

  void set_x(Type x) { x_ = x; }
  void set_y(Type y) { y_ = y; }

  void Offset(Type delta_x, Type delta_y) {
    x_ += delta_x;
    y_ += delta_y;
  }

  Class Scale(float scale) const {
    return Scale(scale, scale);
  }

  Class Scale(float x_scale, float y_scale) const {
    return Class(static_cast<Type>(x_ * x_scale),
                 static_cast<Type>(y_ * y_scale));
  }

  Class Add(const Class& other) const{
    const Class* orig = static_cast<const Class*>(this);
    Class copy = *orig;
    copy.Offset(other.x_, other.y_);
    return copy;
  }

  Class Subtract(const Class& other) const {
    const Class* orig = static_cast<const Class*>(this);
    Class copy = *orig;
    copy.Offset(-other.x_, -other.y_);
    return copy;
  }

  Class Middle(const Class& other) const {
    return Class((x_ + other.x_) / 2, (y_ + other.y_) / 2);
  }

  bool operator==(const Class& rhs) const {
    return x_ == rhs.x_ && y_ == rhs.y_;
  }

  bool operator!=(const Class& rhs) const {
    return !(*this == rhs);
  }

  // A point is less than another point if its y-value is closer
  // to the origin. If the y-values are the same, then point with
  // the x-value closer to the origin is considered less than the
  // other.
  // This comparison is required to use Point in sets, or sorted
  // vectors.
  bool operator<(const Class& rhs) const {
    return (y_ == rhs.y_) ? (x_ < rhs.x_) : (y_ < rhs.y_);
  }

 protected:
  PointBase(Type x, Type y) : x_(x), y_(y) {}
  // Destructor is intentionally made non virtual and protected.
  // Do not make this public.
  ~PointBase() {}

 private:
  Type x_;
  Type y_;
};

}  // namespace gfx

#endif  // UI_GFX_POINT_BASE_H_
