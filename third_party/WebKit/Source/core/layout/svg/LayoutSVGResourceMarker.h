/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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
 */

#ifndef LayoutSVGResourceMarker_h
#define LayoutSVGResourceMarker_h

#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/SVGMarkerElement.h"
#include "platform/geometry/FloatRect.h"

namespace blink {

class LayoutObject;

class LayoutSVGResourceMarker final : public LayoutSVGResourceContainer {
 public:
  explicit LayoutSVGResourceMarker(SVGMarkerElement*);
  ~LayoutSVGResourceMarker() override;

  const char* GetName() const override { return "LayoutSVGResourceMarker"; }

  void RemoveAllClientsFromCache(bool mark_for_invalidation = true) override;
  void RemoveClientFromCache(LayoutObject*,
                             bool mark_for_invalidation = true) override;

  // Calculates marker boundaries, mapped to the target element's coordinate
  // space.
  FloatRect MarkerBoundaries(
      const AffineTransform& marker_transformation) const;
  AffineTransform MarkerTransformation(const FloatPoint& origin,
                                       float angle,
                                       float stroke_width) const;

  AffineTransform LocalToSVGParentTransform() const final {
    return local_to_parent_transform_;
  }
  void SetNeedsTransformUpdate() final;

  // The viewport origin is (0,0) and not the reference point because each
  // marker instance includes the reference in markerTransformation().
  FloatRect Viewport() const { return FloatRect(FloatPoint(), viewport_size_); }

  bool ShouldPaint() const;

  FloatPoint ReferencePoint() const;
  float Angle() const;
  SVGMarkerUnitsType MarkerUnits() const;
  SVGMarkerOrientType OrientType() const;

  static const LayoutSVGResourceType kResourceType = kMarkerResourceType;
  LayoutSVGResourceType ResourceType() const override { return kResourceType; }

 private:
  void UpdateLayout() override;
  SVGTransformChange CalculateLocalTransform() final;

  AffineTransform local_to_parent_transform_;
  FloatSize viewport_size_;
  bool needs_transform_update_;
};

DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(LayoutSVGResourceMarker,
                                      kMarkerResourceType);

}  // namespace blink

#endif
