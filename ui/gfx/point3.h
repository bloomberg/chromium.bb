// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT3_H_
#define UI_GFX_POINT3_H_
#pragma once

#include <cmath>

#include "ui/gfx/point.h"

namespace gfx {

// A point has an x, y and z coordinate.
class Point3f {
 public:
  Point3f() : x_(0), y_(0), z_(0) {}

  Point3f(float x, float y, float z) : x_(x), y_(y), z_(z) {}

  Point3f(const Point& point) : x_(point.x()), y_(point.y()), z_(0) {}

  ~Point3f() {}

  float x() const { return x_; }
  float y() const { return y_; }
  float z() const { return z_; }

  void set_x(float x) { x_ = x; }
  void set_y(float y) { y_ = y; }
  void set_z(float z) { z_ = z; }

  void SetPoint(float x, float y, float z) {
    x_ = x;
    y_ = y;
    z_ = z;
  }

  // Returns the squared euclidean distance between two points.
  float SquaredDistanceTo(const Point3f& other) const {
    float dx = x_ - other.x_;
    float dy = y_ - other.y_;
    float dz = z_ - other.z_;
    return dx * dx + dy * dy + dz * dz;
  }

  Point AsPoint() const {
    return Point(static_cast<int>(std::floor(x_)),
                 static_cast<int>(std::floor(y_)));
  }

 private:
  float x_;
  float y_;
  float z_;

  // copy/assign are allowed.
};

}  // namespace gfx

#endif  // UI_GFX_POINT3_H_
