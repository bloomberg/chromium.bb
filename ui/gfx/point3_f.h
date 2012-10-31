// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT3_F_H_
#define UI_GFX_POINT3_F_H_

#include "ui/gfx/point_f.h"

namespace gfx {

// A point has an x, y and z coordinate.
class Point3F {
 public:
  Point3F() : x_(0), y_(0), z_(0) {}

  Point3F(float x, float y, float z) : x_(x), y_(y), z_(z) {}

  explicit Point3F(const PointF& point) : x_(point.x()), y_(point.y()), z_(0) {}

  ~Point3F() {}

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
  float SquaredDistanceTo(const Point3F& other) const {
    float dx = x_ - other.x_;
    float dy = y_ - other.y_;
    float dz = z_ - other.z_;
    return dx * dx + dy * dy + dz * dz;
  }

  PointF AsPointF() const { return PointF(x_, y_); }

 private:
  float x_;
  float y_;
  float z_;

  // copy/assign are allowed.
};

}  // namespace gfx

#endif  // UI_GFX_POINT3_F_H_
