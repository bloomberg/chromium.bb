// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_RECT_H_
#define PPAPI_CPP_RECT_H_

#include "ppapi/c/pp_rect.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/size.h"

namespace pp {

class Rect {
 public:
  Rect() {
    rect_.point.x = 0;
    rect_.point.y = 0;
    rect_.size.width = 0;
    rect_.size.height = 0;
  }
  Rect(const PP_Rect& rect) {  // Implicit.
    set_x(rect.point.x);
    set_y(rect.point.y);
    set_width(rect.size.width);
    set_height(rect.size.height);
  }
  Rect(int32_t w, int32_t h) {
    set_x(0);
    set_y(0);
    set_width(w);
    set_height(h);
  }
  Rect(int32_t x, int32_t y, int32_t w, int32_t h) {
    set_x(x);
    set_y(y);
    set_width(w);
    set_height(h);
  }
  explicit Rect(const Size& s) {
    set_x(0);
    set_y(0);
    set_size(s);
  }
  Rect(const Point& origin, const Size& size) {
    set_point(origin);
    set_size(size);
  }

  ~Rect() {
  }

  operator PP_Rect() const {
    return rect_;
  }
  const PP_Rect& pp_rect() const {
    return rect_;
  }
  PP_Rect& pp_rect() {
    return rect_;
  }

  int32_t x() const {
    return rect_.point.x;
  }
  void set_x(int32_t in_x) {
    rect_.point.x = in_x;
  }

  int32_t y() const {
    return rect_.point.y;
  }
  void set_y(int32_t in_y) {
    rect_.point.y = in_y;
  }

  int32_t width() const {
    return rect_.size.width;
  }
  void set_width(int32_t w) {
    if (w < 0) {
      PP_DCHECK(w >= 0);
      w = 0;
    }
    rect_.size.width = w;
  }

  int32_t height() const {
    return rect_.size.height;
  }
  void set_height(int32_t h) {
    if (h < 0) {
      PP_DCHECK(h >= 0);
      h = 0;
    }
    rect_.size.height = h;
  }

  Point point() const {
    return Point(rect_.point);
  }
  void set_point(const Point& origin) {
    rect_.point = origin;
  }

  Size size() const {
    return Size(rect_.size);
  }
  void set_size(const Size& s) {
    rect_.size.width = s.width();
    rect_.size.height = s.height();
  }

  int32_t right() const {
    return x() + width();
  }
  int32_t bottom() const {
    return y() + height();
  }

  void SetRect(int32_t x, int32_t y, int32_t w, int32_t h) {
    set_x(x);
    set_y(y);
    set_width(w);
    set_height(h);
  }
  void SetRect(const PP_Rect& rect) {
    rect_ = rect;
  }

  // Shrink the rectangle by a horizontal and vertical distance on all sides.
  void Inset(int32_t horizontal, int32_t vertical) {
    Inset(horizontal, vertical, horizontal, vertical);
  }

  // Shrink the rectangle by the specified amount on each side.
  void Inset(int32_t left, int32_t top, int32_t right, int32_t bottom);

  // Move the rectangle by a horizontal and vertical distance.
  void Offset(int32_t horizontal, int32_t vertical);
  void Offset(const Point& point) {
    Offset(point.x(), point.y());
  }

  // Returns true if the area of the rectangle is zero.
  bool IsEmpty() const {
    return rect_.size.width == 0 && rect_.size.height == 0;
  }

  void swap(Rect& other);

  // Returns true if the point identified by point_x and point_y falls inside
  // this rectangle.  The point (x, y) is inside the rectangle, but the
  // point (x + width, y + height) is not.
  bool Contains(int32_t point_x, int32_t point_y) const;

  // Returns true if the specified point is contained by this rectangle.
  bool Contains(const Point& point) const {
    return Contains(point.x(), point.y());
  }

  // Returns true if this rectangle contains the specified rectangle.
  bool Contains(const Rect& rect) const;

  // Returns true if this rectangle int32_tersects the specified rectangle.
  bool Intersects(const Rect& rect) const;

  // Computes the int32_tersection of this rectangle with the given rectangle.
  Rect Intersect(const Rect& rect) const;

  // Computes the union of this rectangle with the given rectangle.  The union
  // is the smallest rectangle containing both rectangles.
  Rect Union(const Rect& rect) const;

  // Computes the rectangle resulting from subtracting |rect| from |this|.  If
  // |rect| does not intersect completely in either the x- or y-direction, then
  // |*this| is returned. If |rect| contains |this|, then an empty Rect is
  // returned.
  Rect Subtract(const Rect& rect) const;

  // Fits as much of the receiving rectangle int32_to the supplied rectangle as
  // possible, returning the result. For example, if the receiver had
  // a x-location of 2 and a width of 4, and the supplied rectangle had
  // an x-location of 0 with a width of 5, the returned rectangle would have
  // an x-location of 1 with a width of 4.
  Rect AdjustToFit(const Rect& rect) const;

  // Returns the center of this rectangle.
  Point CenterPoint() const;

  // Returns true if this rectangle shares an entire edge (i.e., same width or
  // same height) with the given rectangle, and the rectangles do not overlap.
  bool SharesEdgeWith(const Rect& rect) const;

 private:
  PP_Rect rect_;
};

}  // namespace pp

inline bool operator==(const pp::Rect& lhs, const pp::Rect& rhs) {
  return lhs.x() == rhs.x() &&
         lhs.y() == rhs.y() &&
         lhs.width() == rhs.width() &&
         lhs.height() == rhs.height();
}

inline bool operator!=(const pp::Rect& lhs, const pp::Rect& rhs) {
  return !(lhs == rhs);
}

#endif

