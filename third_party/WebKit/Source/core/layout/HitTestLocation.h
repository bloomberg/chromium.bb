/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
*/

#ifndef HitTestLocation_h
#define HitTestLocation_h

#include "base/memory/scoped_refptr.h"
#include "core/CoreExport.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"

namespace blink {

class FloatRoundedRect;

class CORE_EXPORT HitTestLocation {
  DISALLOW_NEW();

 public:
  // Note that all points are in contents (aka "page") coordinate space for the
  // document that is being hit tested.
  HitTestLocation();
  HitTestLocation(const LayoutPoint&);
  HitTestLocation(const FloatPoint&);
  HitTestLocation(const FloatPoint&, const FloatQuad&);
  // Pass non-zero padding values to perform a rect-based hit test.
  HitTestLocation(const LayoutPoint& center_point,
                  unsigned top_padding,
                  unsigned right_padding,
                  unsigned bottom_padding,
                  unsigned left_padding);
  HitTestLocation(const HitTestLocation&, const LayoutSize& offset);
  HitTestLocation(const HitTestLocation&);
  ~HitTestLocation();
  HitTestLocation& operator=(const HitTestLocation&);

  const LayoutPoint& Point() const { return point_; }
  IntPoint RoundedPoint() const { return RoundedIntPoint(point_); }

  // Rect-based hit test related methods.
  bool IsRectBasedTest() const { return is_rect_based_; }
  bool IsRectilinear() const { return is_rectilinear_; }
  IntRect BoundingBox() const { return bounding_box_; }

  static IntRect RectForPoint(const LayoutPoint&,
                              unsigned top_padding,
                              unsigned right_padding,
                              unsigned bottom_padding,
                              unsigned left_padding);

  bool Intersects(const LayoutRect&) const;
  bool Intersects(const FloatRect&) const;
  bool Intersects(const FloatRoundedRect&) const;
  bool ContainsPoint(const FloatPoint&) const;

  const FloatPoint& TransformedPoint() const { return transformed_point_; }
  const FloatQuad& TransformedRect() const { return transformed_rect_; }

 private:
  template <typename RectType>
  bool IntersectsRect(const RectType&, const RectType& bounding_box) const;
  void Move(const LayoutSize& offset);

  // This is cached forms of the more accurate point and area below.
  LayoutPoint point_;
  IntRect bounding_box_;

  FloatPoint transformed_point_;
  FloatQuad transformed_rect_;

  bool is_rect_based_;
  bool is_rectilinear_;
};

}  // namespace blink

#endif  // HitTestLocation_h
