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
#include "platform/graphics/GraphicsContextStateSaver.h"
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

bool LayoutSVGResourceMasker::prepareEffect(LayoutObject* object, GraphicsContext* context)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(style());
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    clearInvalidationMask();

    FloatRect paintInvalidationRect = object->paintInvalidationRectInLocalCoordinates();
    if (paintInvalidationRect.isEmpty() || !element()->hasChildren())
        return false;

    // Content layer start.
    context->beginTransparencyLayer(1, &paintInvalidationRect);

    return true;
}

void LayoutSVGResourceMasker::finishEffect(LayoutObject* object, GraphicsContext* context)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(style());
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    FloatRect paintInvalidationRect = object->paintInvalidationRectInLocalCoordinates();

    const SVGLayoutStyle& svgStyle = style()->svgStyle();
    ColorFilter maskLayerFilter = svgStyle.maskType() == MT_LUMINANCE
        ? ColorFilterLuminanceToAlpha : ColorFilterNone;
    ColorFilter maskContentFilter = svgStyle.colorInterpolation() == CI_LINEARRGB
        ? ColorFilterSRGBToLinearRGB : ColorFilterNone;

    // Mask layer start.
    context->beginLayer(1, SkXfermode::kDstIn_Mode, &paintInvalidationRect, maskLayerFilter);
    {
        // Draw the mask with color conversion (when needed).
        GraphicsContextStateSaver maskContentSaver(*context);
        context->setColorFilter(maskContentFilter);

        drawMaskForRenderer(context, object->objectBoundingBox());
    }

    // Transfer mask layer -> content layer (DstIn)
    context->endLayer();
    // Transfer content layer -> backdrop (SrcOver)
    context->endLayer();
}

void LayoutSVGResourceMasker::drawMaskForRenderer(GraphicsContext* context, const FloatRect& targetBoundingBox)
{
    ASSERT(context);

    AffineTransform contentTransformation;
    SVGUnitTypes::SVGUnitType contentUnits = toSVGMaskElement(element())->maskContentUnits()->currentValue()->enumValue();
    if (contentUnits == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contentTransformation.translate(targetBoundingBox.x(), targetBoundingBox.y());
        contentTransformation.scaleNonUniform(targetBoundingBox.width(), targetBoundingBox.height());
        context->concatCTM(contentTransformation);
    }

    if (!m_maskContentPicture) {
        SubtreeContentTransformScope contentTransformScope(contentTransformation);
        createPicture(context);
    }

    context->drawPicture(m_maskContentPicture.get());
}

void LayoutSVGResourceMasker::createPicture(GraphicsContext* context)
{
    ASSERT(context);

    // Using strokeBoundingBox (instead of paintInvalidationRectInLocalCoordinates) to avoid the intersection
    // with local clips/mask, which may yield incorrect results when mixing objectBoundingBox and
    // userSpaceOnUse units (http://crbug.com/294900).
    FloatRect bounds = strokeBoundingBox();
    context->beginRecording(bounds);
    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        LayoutObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        const LayoutStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;

        SVGPaintContext::paintSubtree(context, renderer);
    }
    m_maskContentPicture = context->endRecording();
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
