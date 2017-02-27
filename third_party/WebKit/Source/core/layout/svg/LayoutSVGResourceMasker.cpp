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

#include "core/layout/svg/LayoutSVGResourceMasker.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/SVGPaintContext.h"
#include "core/svg/SVGElement.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/paint/PaintRecordBuilder.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

LayoutSVGResourceMasker::LayoutSVGResourceMasker(SVGMaskElement* node)
    : LayoutSVGResourceContainer(node) {}

LayoutSVGResourceMasker::~LayoutSVGResourceMasker() {}

void LayoutSVGResourceMasker::removeAllClientsFromCache(
    bool markForInvalidation) {
  m_cachedPaintRecord.reset();
  m_maskContentBoundaries = FloatRect();
  markAllClientsForInvalidation(markForInvalidation
                                    ? LayoutAndBoundariesInvalidation
                                    : ParentOnlyInvalidation);
}

void LayoutSVGResourceMasker::removeClientFromCache(LayoutObject* client,
                                                    bool markForInvalidation) {
  ASSERT(client);
  markClientForInvalidation(client, markForInvalidation
                                        ? BoundariesInvalidation
                                        : ParentOnlyInvalidation);
}

sk_sp<const PaintRecord> LayoutSVGResourceMasker::createPaintRecord(
    AffineTransform& contentTransformation,
    const FloatRect& targetBoundingBox,
    GraphicsContext& context) {
  SVGUnitTypes::SVGUnitType contentUnits = toSVGMaskElement(element())
                                               ->maskContentUnits()
                                               ->currentValue()
                                               ->enumValue();
  if (contentUnits == SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    contentTransformation.translate(targetBoundingBox.x(),
                                    targetBoundingBox.y());
    contentTransformation.scaleNonUniform(targetBoundingBox.width(),
                                          targetBoundingBox.height());
  }

  if (m_cachedPaintRecord)
    return m_cachedPaintRecord;

  SubtreeContentTransformScope contentTransformScope(contentTransformation);

  // Using strokeBoundingBox instead of visualRectInLocalCoordinates
  // to avoid the intersection with local clips/mask, which may yield incorrect
  // results when mixing objectBoundingBox and userSpaceOnUse units.
  // http://crbug.com/294900
  FloatRect bounds = strokeBoundingBox();

  PaintRecordBuilder builder(bounds, nullptr, &context);

  ColorFilter maskContentFilter =
      style()->svgStyle().colorInterpolation() == CI_LINEARRGB
          ? ColorFilterSRGBToLinearRGB
          : ColorFilterNone;
  builder.context().setColorFilter(maskContentFilter);

  for (const SVGElement& childElement :
       Traversal<SVGElement>::childrenOf(*element())) {
    const LayoutObject* layoutObject = childElement.layoutObject();
    if (!layoutObject || layoutObject->styleRef().display() == EDisplay::None)
      continue;
    SVGPaintContext::paintResourceSubtree(builder.context(), layoutObject);
  }

  m_cachedPaintRecord = builder.endRecording();
  return m_cachedPaintRecord;
}

void LayoutSVGResourceMasker::calculateMaskContentVisualRect() {
  for (const SVGElement& childElement :
       Traversal<SVGElement>::childrenOf(*element())) {
    const LayoutObject* layoutObject = childElement.layoutObject();
    if (!layoutObject || layoutObject->styleRef().display() == EDisplay::None)
      continue;
    m_maskContentBoundaries.unite(
        layoutObject->localToSVGParentTransform().mapRect(
            layoutObject->visualRectInLocalSVGCoordinates()));
  }
}

FloatRect LayoutSVGResourceMasker::resourceBoundingBox(
    const LayoutObject* object) {
  SVGMaskElement* maskElement = toSVGMaskElement(element());
  ASSERT(maskElement);

  FloatRect objectBoundingBox = object->objectBoundingBox();
  FloatRect maskBoundaries = SVGLengthContext::resolveRectangle<SVGMaskElement>(
      maskElement, maskElement->maskUnits()->currentValue()->enumValue(),
      objectBoundingBox);

  // Resource was not layouted yet. Give back clipping rect of the mask.
  if (selfNeedsLayout())
    return maskBoundaries;

  if (m_maskContentBoundaries.isEmpty())
    calculateMaskContentVisualRect();

  FloatRect maskRect = m_maskContentBoundaries;
  if (maskElement->maskContentUnits()->currentValue()->value() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    AffineTransform transform;
    transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
    transform.scaleNonUniform(objectBoundingBox.width(),
                              objectBoundingBox.height());
    maskRect = transform.mapRect(maskRect);
  }

  maskRect.intersect(maskBoundaries);
  return maskRect;
}

}  // namespace blink
