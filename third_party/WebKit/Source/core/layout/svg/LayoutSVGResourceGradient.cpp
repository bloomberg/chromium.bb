/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "core/layout/svg/LayoutSVGResourceGradient.h"

#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

LayoutSVGResourceGradient::LayoutSVGResourceGradient(SVGGradientElement* node)
    : LayoutSVGResourcePaintServer(node),
      m_shouldCollectGradientAttributes(true) {}

void LayoutSVGResourceGradient::removeAllClientsFromCache(
    bool markForInvalidation) {
  m_gradientMap.clear();
  m_shouldCollectGradientAttributes = true;
  markAllClientsForInvalidation(markForInvalidation ? PaintInvalidation
                                                    : ParentOnlyInvalidation);
}

void LayoutSVGResourceGradient::removeClientFromCache(
    LayoutObject* client,
    bool markForInvalidation) {
  ASSERT(client);
  m_gradientMap.erase(client);
  markClientForInvalidation(
      client, markForInvalidation ? PaintInvalidation : ParentOnlyInvalidation);
}

SVGPaintServer LayoutSVGResourceGradient::preparePaintServer(
    const LayoutObject& object) {
  clearInvalidationMask();

  // Validate gradient DOM state before building the actual
  // gradient. This should avoid tearing down the gradient we're
  // currently working on. Preferably the state validation should have
  // no side-effects though.
  if (m_shouldCollectGradientAttributes) {
    if (!collectGradientAttributes())
      return SVGPaintServer::invalid();
    m_shouldCollectGradientAttributes = false;
  }

  // Spec: When the geometry of the applicable element has no width or height
  // and objectBoundingBox is specified, then the given effect (e.g. a gradient
  // or a filter) will be ignored.
  FloatRect objectBoundingBox = object.objectBoundingBox();
  if (gradientUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox &&
      objectBoundingBox.isEmpty())
    return SVGPaintServer::invalid();

  std::unique_ptr<GradientData>& gradientData =
      m_gradientMap.insert(&object, nullptr).storedValue->value;
  if (!gradientData)
    gradientData = WTF::wrapUnique(new GradientData);

  // Create gradient object
  if (!gradientData->gradient) {
    gradientData->gradient = buildGradient();

    // We want the text bounding box applied to the gradient space transform
    // now, so the gradient shader can use it.
    if (gradientUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox &&
        !objectBoundingBox.isEmpty()) {
      gradientData->userspaceTransform.translate(objectBoundingBox.x(),
                                                 objectBoundingBox.y());
      gradientData->userspaceTransform.scaleNonUniform(
          objectBoundingBox.width(), objectBoundingBox.height());
    }

    AffineTransform gradientTransform = calculateGradientTransform();
    gradientData->userspaceTransform *= gradientTransform;
  }

  if (!gradientData->gradient)
    return SVGPaintServer::invalid();

  return SVGPaintServer(gradientData->gradient,
                        gradientData->userspaceTransform);
}

bool LayoutSVGResourceGradient::isChildAllowed(LayoutObject* child,
                                               const ComputedStyle&) const {
  if (child->isSVGGradientStop())
    return true;

  if (!child->isSVGResourceContainer())
    return false;

  return toLayoutSVGResourceContainer(child)->isSVGPaintServer();
}

GradientSpreadMethod LayoutSVGResourceGradient::platformSpreadMethodFromSVGType(
    SVGSpreadMethodType method) {
  switch (method) {
    case SVGSpreadMethodUnknown:
    case SVGSpreadMethodPad:
      return SpreadMethodPad;
    case SVGSpreadMethodReflect:
      return SpreadMethodReflect;
    case SVGSpreadMethodRepeat:
      return SpreadMethodRepeat;
  }

  NOTREACHED();
  return SpreadMethodPad;
}

}  // namespace blink
