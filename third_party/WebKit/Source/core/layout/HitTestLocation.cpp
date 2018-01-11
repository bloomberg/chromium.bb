/*
 * Copyright (C) 2006, 2008, 2011 Apple Inc. All rights reserved.
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

#include "core/layout/HitTestLocation.h"

#include "platform/geometry/FloatRoundedRect.h"

namespace blink {

HitTestLocation::HitTestLocation()
    : is_rect_based_(false), is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const LayoutPoint& point)
    : point_(point),
      bounding_box_(RectForPoint(point, 0, 0, 0, 0)),
      transformed_point_(point),
      transformed_rect_(bounding_box_),
      is_rect_based_(false),
      is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const FloatPoint& point)
    : point_(FlooredLayoutPoint(point)),
      bounding_box_(RectForPoint(point_, 0, 0, 0, 0)),
      transformed_point_(point),
      transformed_rect_(bounding_box_),
      is_rect_based_(false),
      is_rectilinear_(true) {}

HitTestLocation::HitTestLocation(const FloatPoint& point, const FloatQuad& quad)
    : transformed_point_(point), transformed_rect_(quad), is_rect_based_(true) {
  point_ = FlooredLayoutPoint(point);
  bounding_box_ = EnclosingIntRect(quad.BoundingBox());
  is_rectilinear_ = quad.IsRectilinear();
}

HitTestLocation::HitTestLocation(const LayoutPoint& center_point,
                                 unsigned top_padding,
                                 unsigned right_padding,
                                 unsigned bottom_padding,
                                 unsigned left_padding)
    : point_(center_point),
      bounding_box_(RectForPoint(center_point,
                                 top_padding,
                                 right_padding,
                                 bottom_padding,
                                 left_padding)),
      transformed_point_(center_point),
      is_rect_based_(top_padding || right_padding || bottom_padding ||
                     left_padding),
      is_rectilinear_(true) {
  transformed_rect_ = FloatQuad(bounding_box_);
}

HitTestLocation::HitTestLocation(const HitTestLocation& other,
                                 const LayoutSize& offset)
    : point_(other.point_),
      bounding_box_(other.bounding_box_),
      transformed_point_(other.transformed_point_),
      transformed_rect_(other.transformed_rect_),
      is_rect_based_(other.is_rect_based_),
      is_rectilinear_(other.is_rectilinear_) {
  Move(offset);
}

HitTestLocation::HitTestLocation(const HitTestLocation& other) = default;

HitTestLocation::~HitTestLocation() = default;

HitTestLocation& HitTestLocation::operator=(const HitTestLocation& other) =
    default;

void HitTestLocation::Move(const LayoutSize& offset) {
  point_.Move(offset);
  transformed_point_.Move(offset);
  transformed_rect_.Move(offset);
  bounding_box_ = EnclosingIntRect(transformed_rect_.BoundingBox());
}

template <typename RectType>
bool HitTestLocation::IntersectsRect(const RectType& rect,
                                     const RectType& bounding_box) const {
  // FIXME: When the hit test is not rect based we should use
  // rect.contains(m_point).
  // That does change some corner case tests though.

  // First check if rect even intersects our bounding box.
  if (!rect.Intersects(bounding_box))
    return false;

  // If the transformed rect is rectilinear the bounding box intersection was
  // accurate.
  if (is_rectilinear_)
    return true;

  // If rect fully contains our bounding box, we are also sure of an
  // intersection.
  if (rect.Contains(bounding_box))
    return true;

  // Otherwise we need to do a slower quad based intersection test.
  return transformed_rect_.IntersectsRect(FloatRect(rect));
}

bool HitTestLocation::Intersects(const LayoutRect& rect) const {
  return IntersectsRect(rect, LayoutRect(bounding_box_));
}

bool HitTestLocation::Intersects(const FloatRect& rect) const {
  return IntersectsRect(rect, FloatRect(bounding_box_));
}

bool HitTestLocation::Intersects(const FloatRoundedRect& rect) const {
  return rect.IntersectsQuad(transformed_rect_);
}

bool HitTestLocation::ContainsPoint(const FloatPoint& point) const {
  return transformed_rect_.ContainsPoint(point);
}

IntRect HitTestLocation::RectForPoint(const LayoutPoint& point,
                                      unsigned top_padding,
                                      unsigned right_padding,
                                      unsigned bottom_padding,
                                      unsigned left_padding) {
  IntPoint actual_point(FlooredIntPoint(point));
  actual_point -= IntSize(left_padding, top_padding);

  IntSize actual_padding(left_padding + right_padding,
                         top_padding + bottom_padding);
  // As IntRect is left inclusive and right exclusive (seeing
  // IntRect::contains(x, y)), adding "1".
  // FIXME: Remove this once non-rect based hit-detection stops using
  // IntRect:intersects.
  actual_padding += IntSize(1, 1);

  return IntRect(actual_point, actual_padding);
}

}  // namespace blink
