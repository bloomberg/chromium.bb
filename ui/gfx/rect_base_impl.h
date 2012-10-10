// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/rect_base.h"

#include "base/logging.h"
#include "base/stringprintf.h"

// This file provides the implementation for RectBaese template and
// used to instantiate the base class for Rect and RectF classes.
#if !defined(UI_IMPLEMENTATION)
#error "This file is intended for UI implementation only"
#endif

namespace {

template<typename Type>
void AdjustAlongAxis(Type dst_origin, Type dst_size, Type* origin, Type* size) {
  *size = std::min(dst_size, *size);
  if (*origin < dst_origin)
    *origin = dst_origin;
  else
    *origin = std::min(dst_origin + dst_size, *origin + *size) - *size;
}

} // namespace

namespace gfx {

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::RectBase(
    const PointClass& origin, const SizeClass& size)
    : origin_(origin), size_(size) {
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::RectBase(
    const SizeClass& size)
    : size_(size) {
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::RectBase(
    const PointClass& origin)
    : origin_(origin) {
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::~RectBase() {}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::SetRect(
    Type x, Type y, Type width, Type height) {
  origin_.SetPoint(x, y);
  set_width(width);
  set_height(height);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Inset(
    const InsetsClass& insets) {
  Inset(insets.left(), insets.top(), insets.right(), insets.bottom());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Inset(
    Type left, Type top, Type right, Type bottom) {
  Offset(left, top);
  set_width(std::max(width() - left - right, static_cast<Type>(0)));
  set_height(std::max(height() - top - bottom, static_cast<Type>(0)));
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Offset(
    Type horizontal, Type vertical) {
  origin_.Offset(horizontal, vertical);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::operator==(
    const Class& other) const {
  return origin_ == other.origin_ && size_ == other.size_;
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::operator<(
    const Class& other) const {
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

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Contains(
    Type point_x, Type point_y) const {
  return (point_x >= x()) && (point_x < right()) &&
         (point_y >= y()) && (point_y < bottom());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Contains(
    const Class& rect) const {
  return (rect.x() >= x() && rect.right() <= right() &&
          rect.y() >= y() && rect.bottom() <= bottom());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Intersects(
    const Class& rect) const {
  return !(rect.x() >= right() || rect.right() <= x() ||
           rect.y() >= bottom() || rect.bottom() <= y());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
Class RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Intersect(
    const Class& rect) const {
  Type rx = std::max(x(), rect.x());
  Type ry = std::max(y(), rect.y());
  Type rr = std::min(right(), rect.right());
  Type rb = std::min(bottom(), rect.bottom());

  if (rx >= rr || ry >= rb)
    rx = ry = rr = rb = 0;  // non-intersecting

  return Class(rx, ry, rr - rx, rb - ry);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
Class RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Union(
    const Class& rect) const {
  // special case empty rects...
  if (IsEmpty())
    return rect;
  if (rect.IsEmpty())
    return *static_cast<const Class*>(this);

  Type rx = std::min(x(), rect.x());
  Type ry = std::min(y(), rect.y());
  Type rr = std::max(right(), rect.right());
  Type rb = std::max(bottom(), rect.bottom());

  return Class(rx, ry, rr - rx, rb - ry);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
Class RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Subtract(
    const Class& rect) const {
  // boundary cases:
  if (!Intersects(rect))
    return *static_cast<const Class*>(this);
  if (rect.Contains(*static_cast<const Class*>(this)))
    return Class();

  Type rx = x();
  Type ry = y();
  Type rr = right();
  Type rb = bottom();

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
  return Class(rx, ry, rr - rx, rb - ry);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
Class RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::AdjustToFit(
    const Class& rect) const {
  Type new_x = x();
  Type new_y = y();
  Type new_width = width();
  Type new_height = height();
  AdjustAlongAxis(rect.x(), rect.width(), &new_x, &new_width);
  AdjustAlongAxis(rect.y(), rect.height(), &new_y, &new_height);
  return Class(new_x, new_y, new_width, new_height);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
PointClass RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::
    CenterPoint() const {
  return PointClass(x() + (width() - 1) / 2, y() + (height() - 1) / 2);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
Class RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::Center(
    const SizeClass& size) const {
  Type new_width = std::min(width(), size.width());
  Type new_height = std::min(height(), size.height());
  Type new_x = x() + (width() - new_width) / 2;
  Type new_y = y() + (height() - new_height) / 2;
  return Class(new_x, new_y, new_width, new_height);
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
void RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::SplitVertically(
    Class* left_half, Class* right_half) const {
  DCHECK(left_half);
  DCHECK(right_half);

  left_half->SetRect(this->x(), this->y(), this->width() / 2, this->height());
  right_half->SetRect(left_half->right(),
                      this->y(),
                      this->width() - left_half->width(),
                      this->height());
}

template<typename Class,
         typename PointClass,
         typename SizeClass,
         typename InsetsClass,
         typename Type>
bool RectBase<Class, PointClass, SizeClass, InsetsClass, Type>::SharesEdgeWith(
    const Class& rect) const {
  return (y() == rect.y() && height() == rect.height() &&
             (x() == rect.right() || right() == rect.x())) ||
         (x() == rect.x() && width() == rect.width() &&
             (y() == rect.bottom() || bottom() == rect.y()));
}

}  // namespace gfx
