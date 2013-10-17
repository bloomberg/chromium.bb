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

#include "core/rendering/svg/RenderSVGResourceMasker.h"

#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/GraphicsContextStateSaver.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGMaskElement.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/geometry/FloatRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/UnusedParam.h"
#include "wtf/Vector.h"

namespace WebCore {

RenderSVGResourceType RenderSVGResourceMasker::s_resourceType = MaskerResourceType;

RenderSVGResourceMasker::RenderSVGResourceMasker(SVGMaskElement* node)
    : RenderSVGResourceContainer(node, RenderSVGResourceMaskerObjectType)
{
}

RenderSVGResourceMasker::~RenderSVGResourceMasker()
{
}

void RenderSVGResourceMasker::removeAllClientsFromCache(bool markForInvalidation)
{
    m_maskContentBoundaries = FloatRect();
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceMasker::removeClientFromCache(RenderObject* client, bool markForInvalidation)
{
    ASSERT(client);
    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool RenderSVGResourceMasker::applyResource(RenderObject* object, RenderStyle*,
    GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(style());
    ASSERT_UNUSED(resourceMode, resourceMode == ApplyToDefaultMode);
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    FloatRect repaintRect = object->repaintRectInLocalCoordinates();
    if (repaintRect.isEmpty() || !element()->hasChildNodes())
        return false;

    // Content layer start.
    context->beginTransparencyLayer(1, &repaintRect);

    return true;
}

void RenderSVGResourceMasker::postApplyResource(RenderObject* object, GraphicsContext*& context,
    unsigned short resourceMode, const Path*, const RenderSVGShape*)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(style());
    ASSERT_UNUSED(resourceMode, resourceMode == ApplyToDefaultMode);
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    FloatRect repaintRect = object->repaintRectInLocalCoordinates();

    const SVGRenderStyle* svgStyle = style()->svgStyle();
    ASSERT(svgStyle);
    ColorFilter maskLayerFilter = svgStyle->maskType() == MT_LUMINANCE
        ? ColorFilterLuminanceToAlpha : ColorFilterNone;
    ColorFilter maskContentFilter = svgStyle->colorInterpolation() == CI_LINEARRGB
        ? ColorFilterSRGBToLinearRGB : ColorFilterNone;

    // Mask layer start.
    context->beginLayer(1, CompositeDestinationIn, &repaintRect, maskLayerFilter);
    {
        // Draw the mask with color conversion (when needed).
        GraphicsContextStateSaver maskContentSaver(*context);
        context->setColorFilter(maskContentFilter);

        drawMaskContent(context, object->objectBoundingBox());
    }

    // Transfer mask layer -> content layer (DstIn)
    context->endLayer();
    // Transfer content layer -> backdrop (SrcOver)
    context->endLayer();
}

void RenderSVGResourceMasker::drawMaskContent(GraphicsContext* context, const FloatRect& targetBoundingBox)
{
    ASSERT(context);

    // Adjust the mask image context according to the target objectBoundingBox.
    AffineTransform maskContentTransformation;
    if (toSVGMaskElement(element())->maskContentUnitsCurrentValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskContentTransformation.translate(targetBoundingBox.x(), targetBoundingBox.y());
        maskContentTransformation.scaleNonUniform(targetBoundingBox.width(), targetBoundingBox.height());
        context->concatCTM(maskContentTransformation);
    }

    for (Node* childNode = element()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;

        SVGRenderingContext::renderSubtree(context, renderer, maskContentTransformation);
    }
}

void RenderSVGResourceMasker::calculateMaskContentRepaintRect()
{
    for (Node* childNode = element()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
             continue;
        m_maskContentBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates()));
    }
}

FloatRect RenderSVGResourceMasker::resourceBoundingBox(RenderObject* object)
{
    SVGMaskElement* maskElement = toSVGMaskElement(element());
    ASSERT(maskElement);

    FloatRect objectBoundingBox = object->objectBoundingBox();
    FloatRect maskBoundaries = SVGLengthContext::resolveRectangle<SVGMaskElement>(maskElement, maskElement->maskUnitsCurrentValue(), objectBoundingBox);

    // Resource was not layouted yet. Give back clipping rect of the mask.
    if (selfNeedsLayout())
        return maskBoundaries;

    if (m_maskContentBoundaries.isEmpty())
        calculateMaskContentRepaintRect();

    FloatRect maskRect = m_maskContentBoundaries;
    if (maskElement->maskContentUnitsCurrentValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        maskRect = transform.mapRect(maskRect);
    }

    maskRect.intersect(maskBoundaries);
    return maskRect;
}

}
