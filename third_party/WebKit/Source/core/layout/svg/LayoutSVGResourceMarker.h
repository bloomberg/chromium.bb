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

  const char* name() const override { return "LayoutSVGResourceMarker"; }

  void removeAllClientsFromCache(bool markForInvalidation = true) override;
  void removeClientFromCache(LayoutObject*,
                             bool markForInvalidation = true) override;

  // Calculates marker boundaries, mapped to the target element's coordinate
  // space.
  FloatRect markerBoundaries(const AffineTransform& markerTransformation) const;
  AffineTransform markerTransformation(const FloatPoint& origin,
                                       float angle,
                                       float strokeWidth) const;

  AffineTransform localToSVGParentTransform() const final {
    return m_localToParentTransform;
  }
  void setNeedsTransformUpdate() final;

  // The viewport origin is (0,0) and not the reference point because each
  // marker instance includes the reference in markerTransformation().
  FloatRect viewport() const { return FloatRect(FloatPoint(), m_viewportSize); }

  bool shouldPaint() const;

  FloatPoint referencePoint() const;
  float angle() const;
  SVGMarkerUnitsType markerUnits() const;
  SVGMarkerOrientType orientType() const;

  static const LayoutSVGResourceType s_resourceType = MarkerResourceType;
  LayoutSVGResourceType resourceType() const override { return s_resourceType; }

 private:
  void layout() override;
  SVGTransformChange calculateLocalTransform() final;

  AffineTransform m_localToParentTransform;
  FloatSize m_viewportSize;
  bool m_needsTransformUpdate;
};

DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(LayoutSVGResourceMarker,
                                      MarkerResourceType);

}  // namespace blink

#endif
