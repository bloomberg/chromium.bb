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

#include "config.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/paint/SVGPaintContext.h"
#include "core/svg/SVGElement.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/transforms/AffineTransform.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

LayoutSVGResourceMasker::LayoutSVGResourceMasker(SVGMaskElement* node)
    : LayoutSVGResourceContainer(node)
{
}

LayoutSVGResourceMasker::~LayoutSVGResourceMasker()
{
}

void LayoutSVGResourceMasker::removeAllClientsFromCache(bool markForInvalidation)
{
    m_maskContentPicture.clear();
    m_maskContentBoundaries = FloatRect();
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void LayoutSVGResourceMasker::removeClientFromCache(LayoutObject* client, bool markForInvalidation)
{
    ASSERT(client);
    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

PassRefPtr<const SkPicture> LayoutSVGResourceMasker::createContentPicture(AffineTransform& contentTransformation, const FloatRect& targetBoundingBox)
{
    SVGUnitTypes::SVGUnitType contentUnits = toSVGMaskElement(element())->maskContentUnits()->currentValue()->enumValue();
    if (contentUnits == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contentTransformation.translate(targetBoundingBox.x(), targetBoundingBox.y());
        contentTransformation.scaleNonUniform(targetBoundingBox.width(), targetBoundingBox.height());
    }

    if (m_maskContentPicture)
        return m_maskContentPicture;

    SubtreeContentTransformScope contentTransformScope(contentTransformation);

    // Using strokeBoundingBox (instead of paintInvalidationRectInLocalCoordinates) to avoid the intersection
    // with local clips/mask, which may yield incorrect results when mixing objectBoundingBox and
    // userSpaceOnUse units (http://crbug.com/294900).
    FloatRect bounds = strokeBoundingBox();

    OwnPtr<DisplayItemList> displayItemList;
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        displayItemList = DisplayItemList::create();
    GraphicsContext context(nullptr, displayItemList.get());
    context.beginRecording(bounds);

    ColorFilter maskContentFilter = style()->svgStyle().colorInterpolation() == CI_LINEARRGB
        ? ColorFilterSRGBToLinearRGB : ColorFilterNone;
    context.setColorFilter(maskContentFilter);

    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        LayoutObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        const LayoutStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;

        SVGPaintContext::paintSubtree(&context, renderer);
    }

    if (displayItemList)
        displayItemList->replay(&context);
    m_maskContentPicture = context.endRecording();
    return m_maskContentPicture;
}

void LayoutSVGResourceMasker::calculateMaskContentPaintInvalidationRect()
{
    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        LayoutObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        const LayoutStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;
        m_maskContentBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->paintInvalidationRectInLocalCoordinates()));
    }
}

FloatRect LayoutSVGResourceMasker::resourceBoundingBox(const LayoutObject* object)
{
    SVGMaskElement* maskElement = toSVGMaskElement(element());
    ASSERT(maskElement);

    FloatRect objectBoundingBox = object->objectBoundingBox();
    FloatRect maskBoundaries = SVGLengthContext::resolveRectangle<SVGMaskElement>(maskElement, maskElement->maskUnits()->currentValue()->enumValue(), objectBoundingBox);

    // Resource was not layouted yet. Give back clipping rect of the mask.
    if (selfNeedsLayout())
        return maskBoundaries;

    if (m_maskContentBoundaries.isEmpty())
        calculateMaskContentPaintInvalidationRect();

    FloatRect maskRect = m_maskContentBoundaries;
    if (maskElement->maskContentUnits()->currentValue()->value() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        maskRect = transform.mapRect(maskRect);
    }

    maskRect.intersect(maskBoundaries);
    return maskRect;
}

}
