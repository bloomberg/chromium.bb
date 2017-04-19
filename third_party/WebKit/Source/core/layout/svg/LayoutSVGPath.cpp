/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2005, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2009 Jeff Schiller <codedread@gmail.com>
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * Copyright (C) 2011 University of Szeged
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

#include "core/layout/svg/LayoutSVGPath.h"

#include "core/layout/svg/LayoutSVGResourceMarker.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/svg/SVGGeometryElement.h"
#include "platform/wtf/MathExtras.h"

namespace blink {

LayoutSVGPath::LayoutSVGPath(SVGGeometryElement* node) : LayoutSVGShape(node) {}

LayoutSVGPath::~LayoutSVGPath() {}

void LayoutSVGPath::UpdateShapeFromElement() {
  LayoutSVGShape::UpdateShapeFromElement();
  ProcessMarkerPositions();

  stroke_bounding_box_ = CalculateUpdatedStrokeBoundingBox();
  if (GetElement())
    GetElement()->SetNeedsResizeObserverUpdate();
}

FloatRect LayoutSVGPath::HitTestStrokeBoundingBox() const {
  const SVGComputedStyle& svg_style = Style()->SvgStyle();
  if (svg_style.HasStroke())
    return stroke_bounding_box_;

  // Implementation of
  // http://dev.w3.org/fxtf/css-masking-1/#compute-stroke-bounding-box
  // except that we ignore whether the stroke is none.

  FloatRect box = fill_bounding_box_;

  const float stroke_width = this->StrokeWidth();
  if (stroke_width <= 0)
    return box;

  float delta = stroke_width / 2;

  if (svg_style.HasMiterJoinStyle()) {
    const float miter = svg_style.StrokeMiterLimit();
    if (miter < M_SQRT2 && svg_style.HasSquareCapStyle())
      delta *= M_SQRT2;
    else
      delta *= miter;
  } else if (svg_style.HasSquareCapStyle()) {
    delta *= M_SQRT2;
  }

  box.Inflate(delta);
  return box;
}

FloatRect LayoutSVGPath::CalculateUpdatedStrokeBoundingBox() const {
  FloatRect stroke_bounding_box = stroke_bounding_box_;
  if (!marker_positions_.IsEmpty())
    stroke_bounding_box.Unite(MarkerRect(StrokeWidth()));
  return stroke_bounding_box;
}

FloatRect LayoutSVGPath::MarkerRect(float stroke_width) const {
  DCHECK(!marker_positions_.IsEmpty());

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(this);
  DCHECK(resources);

  LayoutSVGResourceMarker* marker_start = resources->MarkerStart();
  LayoutSVGResourceMarker* marker_mid = resources->MarkerMid();
  LayoutSVGResourceMarker* marker_end = resources->MarkerEnd();
  DCHECK(marker_start || marker_mid || marker_end);

  FloatRect boundaries;
  unsigned size = marker_positions_.size();
  for (unsigned i = 0; i < size; ++i) {
    if (LayoutSVGResourceMarker* marker = SVGMarkerData::MarkerForType(
            marker_positions_[i].type, marker_start, marker_mid, marker_end))
      boundaries.Unite(marker->MarkerBoundaries(marker->MarkerTransformation(
          marker_positions_[i].origin, marker_positions_[i].angle,
          stroke_width)));
  }
  return boundaries;
}

bool LayoutSVGPath::ShouldGenerateMarkerPositions() const {
  if (!Style()->SvgStyle().HasMarkers())
    return false;

  if (!SVGResources::SupportsMarkers(*ToSVGGraphicsElement(GetElement())))
    return false;

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(this);
  if (!resources)
    return false;

  return resources->MarkerStart() || resources->MarkerMid() ||
         resources->MarkerEnd();
}

void LayoutSVGPath::ProcessMarkerPositions() {
  marker_positions_.clear();

  if (!ShouldGenerateMarkerPositions())
    return;

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(this);
  DCHECK(resources);

  LayoutSVGResourceMarker* marker_start = resources->MarkerStart();

  SVGMarkerData marker_data(marker_positions_,
                            marker_start ? marker_start->OrientType() ==
                                               kSVGMarkerOrientAutoStartReverse
                                         : false);
  GetPath().Apply(&marker_data, SVGMarkerData::UpdateFromPathElement);
  marker_data.PathIsDone();
}

}  // namespace blink
