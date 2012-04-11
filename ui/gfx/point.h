// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_POINT_H_
#define UI_GFX_POINT_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/base/ui_export.h"

#if defined(OS_WIN)
typedef unsigned long DWORD;
typedef struct tagPOINT POINT;
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#endif

namespace gfx {

// A point has an x and y coordinate.
class UI_EXPORT Point {
 public:
  Point();
  Point(int x, int y);
#if defined(OS_WIN)
  // |point| is a DWORD value that contains a coordinate.  The x-coordinate is
  // the low-order short and the y-coordinate is the high-order short.  This
  // value is commonly acquired from GetMessagePos/GetCursorPos.
  explicit Point(DWORD point);
  explicit Point(const POINT& point);
  Point& operator=(const POINT& point);
#elif defined(OS_MACOSX)
  explicit Point(const CGPoint& point);
#endif

  ~Point() {}

  int x() const { return x_; }
  int y() const { return y_; }

  void SetPoint(int x, int y) {
    x_ = x;
    y_ = y;
  }

  void set_x(int x) { x_ = x; }
  void set_y(int y) { y_ = y; }

  void Offset(int delta_x, int delta_y) {
    x_ += delta_x;
    y_ += delta_y;
  }

  Point Add(const Point& other) const{
    Point copy = *this;
    copy.Offset(other.x_, other.y_);
    return copy;
  }

  Point Subtract(const Point& other) const {
    Point copy = *this;
    copy.Offset(-other.x_, -other.y_);
    return copy;
  }

  Point Middle(const Point& other) const {
    return Point((x_ + other.x_) / 2, (y_ + other.y_) / 2);
  }

  bool operator==(const Point& rhs) const {
    return x_ == rhs.x_ && y_ == rhs.y_;
  }

  bool operator!=(const Point& rhs) const {
    return !(*this == rhs);
  }

  // A point is less than another point if its y-value is closer
  // to the origin. If the y-values are the same, then point with
  // the x-value closer to the origin is considered less than the
  // other.
  // This comparison is required to use Points in sets, or sorted
  // vectors.
  bool operator<(const Point& rhs) const {
    return (y_ == rhs.y_) ? (x_ < rhs.x_) : (y_ < rhs.y_);
  }

#if defined(OS_WIN)
  POINT ToPOINT() const;
#elif defined(OS_MACOSX)
  CGPoint ToCGPoint() const;
#endif

  // Returns a string representation of point.
  std::string ToString() const;

 private:
  int x_;
  int y_;
};

}  // namespace gfx

#endif  // UI_GFX_POINT_H_
