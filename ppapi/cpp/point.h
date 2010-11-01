// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_POINT_H_
#define PPAPI_CPP_POINT_H_

#include "ppapi/c/pp_point.h"

namespace pp {

// A point has an x and y coordinate.
class Point {
 public:
  Point() {
    point_.x = 0;
    point_.y = 0;
  }
  Point(int32_t in_x, int32_t in_y) {
    point_.x = in_x;
    point_.y = in_y;
  }
  Point(const PP_Point& point) {  // Implicit.
    point_.x = point.x;
    point_.y = point.y;
  }

  ~Point() {
  }

  operator PP_Point() const {
    return point_;
  }
  const PP_Point& pp_point() const {
    return point_;
  }
  PP_Point& pp_point() {
    return point_;
  }

  int32_t x() const { return point_.x; }
  void set_x(int32_t in_x) {
    point_.x = in_x;
  }

  int32_t y() const { return point_.y; }
  void set_y(int32_t in_y) {
    point_.y = in_y;
  }

  Point operator+(const Point& other) const {
    return Point(x() + other.x(), y() + other.y());
  }
  Point operator-(const Point& other) const {
    return Point(x() - other.x(), y() - other.y());
  }

  Point& operator+=(const Point& other) {
    point_.x += other.x();
    point_.y += other.y();
    return *this;
  }
  Point& operator-=(const Point& other) {
    point_.x -= other.x();
    point_.y -= other.y();
    return *this;
  }

  void swap(Point& other) {
    int32_t x = point_.x;
    int32_t y = point_.y;
    point_.x = other.point_.x;
    point_.y = other.point_.y;
    other.point_.x = x;
    other.point_.y = y;
  }

 private:
  PP_Point point_;
};

}  // namespace pp

inline bool operator==(const pp::Point& lhs, const pp::Point& rhs) {
  return lhs.x() == rhs.x() && lhs.y() == rhs.y();
}

inline bool operator!=(const pp::Point& lhs, const pp::Point& rhs) {
  return !(lhs == rhs);
}

#endif  // PPAPI_CPP_POINT_H_

