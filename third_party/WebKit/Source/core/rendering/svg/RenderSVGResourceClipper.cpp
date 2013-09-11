/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Dirk Schulze <krit@webkit.org>
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

#include "core/rendering/svg/RenderSVGResourceClipper.h"

#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/platform/graphics/GraphicsContextStateSaver.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/svg/SVGUseElement.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

RenderSVGResourceType RenderSVGResourceClipper::s_resourceType = ClipperResourceType;

RenderSVGResourceClipper::RenderSVGResourceClipper(SVGClipPathElement* node)
    : RenderSVGResourceContainer(node)
    , m_inClipExpansion(false)
{
}

RenderSVGResourceClipper::~RenderSVGResourceClipper()
{
}

void RenderSVGResourceClipper::removeAllClientsFromCache(bool markForInvalidation)
{
    m_clipBoundaries = FloatRect();
    m_rendererToClipperMap.clear();
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void RenderSVGResourceClipper::removeClientFromCache(RenderObject* client, bool markForInvalidation)
{
    ASSERT(client);
    m_rendererToClipperMap.remove(client);
    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool RenderSVGResourceClipper::applyResource(RenderObject* object, RenderStyle*, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT_UNUSED(resourceMode, resourceMode == ApplyToDefaultMode);

    return applyClippingToContext(object, object->objectBoundingBox(), object->repaintRectInLocalCoordinates(), context);
}

bool RenderSVGResourceClipper::tryPathOnlyClipping(GraphicsContext* context,
    const AffineTransform& animatedLocalTransform, const FloatRect& objectBoundingBox) {
    // If the current clip-path gets clipped itself, we have to fallback to masking.
    if (!style()->svgStyle()->clipperResource().isEmpty())
        return false;
    WindRule clipRule = RULE_NONZERO;
    Path clipPath = Path();

    for (Node* childNode = element()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!renderer)
            continue;
        // Only shapes or paths are supported for direct clipping. We need to fallback to masking for texts.
        if (renderer->isSVGText())
            return false;
        if (!childNode->isSVGElement() || !toSVGElement(childNode)->isSVGGraphicsElement())
            continue;
        SVGGraphicsElement* styled = toSVGGraphicsElement(childNode);
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
             continue;
        const SVGRenderStyle* svgStyle = style->svgStyle();
        // Current shape in clip-path gets clipped too. Fallback to masking.
        if (!svgStyle->clipperResource().isEmpty())
            return false;

        if (clipPath.isEmpty()) {
            // First clip shape.
            styled->toClipPath(clipPath);
            clipRule = svgStyle->clipRule();
            clipPath.setWindRule(clipRule);
            continue;
        }

        if (RuntimeEnabledFeatures::pathOpsSVGClippingEnabled()) {
            // Attempt to generate a combined clip path, fall back to masking if not possible.
            Path subPath;
            styled->toClipPath(subPath);
            subPath.setWindRule(svgStyle->clipRule());
            if (!clipPath.unionPath(subPath))
                return false;
        } else {
            return false;
        }
    }
    // Only one visible shape/path was found. Directly continue clipping and transform the content to userspace if necessary.
    if (toSVGClipPathElement(element())->clipPathUnitsCurrentValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        clipPath.transform(transform);
    }

    // Transform path by animatedLocalTransform.
    clipPath.transform(animatedLocalTransform);

    // The SVG specification wants us to clip everything, if clip-path doesn't have a child.
    if (clipPath.isEmpty())
        clipPath.addRect(FloatRect());
    context->clipPath(clipPath, clipRule);
    return true;
}

bool RenderSVGResourceClipper::applyClippingToContext(RenderObject* target, const FloatRect& targetBoundingBox,
                                                      const FloatRect& repaintRect, GraphicsContext* context)
{
    ASSERT(target);
    ASSERT(context);
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    if (repaintRect.isEmpty() || m_inClipExpansion)
        return false;
    TemporaryChange<bool> inClipExpansionChange(m_inClipExpansion, true);

    // add(obj, 0) idiom -> lookup & add if not found.
    OwnPtr<ClipperData>& clipperData = m_rendererToClipperMap.add(target, nullptr).iterator->value;
    if (!clipperData)
        clipperData = adoptPtr(new ClipperData);

    // First, try to apply the clip as a clipPath.
    AffineTransform animatedLocalTransform = toSVGClipPathElement(element())->animatedLocalTransform();
    if (tryPathOnlyClipping(context, animatedLocalTransform, targetBoundingBox)) {
        clipperData->clipMode = ClipperData::PathOnlyClipMode;
        return true;
    }

    // Fall back to masking.
    clipperData->clipMode = ClipperData::MaskClipMode;

    // Mask layer start
    context->beginTransparencyLayer(1, &repaintRect);
    {
        GraphicsContextStateSaver maskContentSaver(*context);
        context->concatCTM(animatedLocalTransform);

        // clipPath can also be clipped by another clipPath.
        SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(this);
        RenderSVGResourceClipper* clipPathClipper = 0;
        if (resources && (clipPathClipper = resources->clipper())) {
            if (!clipPathClipper->applyClippingToContext(this, targetBoundingBox, repaintRect, context)) {
                // FIXME: Awkward state micro-management. Ideally, GraphicsContextStateSaver should
                //   a) pop saveLayers also
                //   b) pop multiple states if needed (similarly to SkCanvas::restoreToCount())
                // Then we should be able to replace this mess with a single, top-level GCSS.
                maskContentSaver.restore();
                context->restoreLayer();
                return false;
            }
        }

        drawMaskContent(context, targetBoundingBox);

        if (clipPathClipper)
            clipPathClipper->postApplyResource(this, context, ApplyToDefaultMode, 0, 0);
    }

    // Masked content layer start.
    context->beginMaskedLayer(repaintRect);

    return true;
}

void RenderSVGResourceClipper::postApplyResource(RenderObject* object, GraphicsContext*& context,
    unsigned short resourceMode, const Path*, const RenderSVGShape*) {
    ASSERT_UNUSED(resourceMode, resourceMode == ApplyToDefaultMode);

    ClipperData* clipperData = m_rendererToClipperMap.get(object);
    ASSERT(clipperData);

    // Path-only clipping, no layers to restore.
    if (clipperData->clipMode == ClipperData::PathOnlyClipMode)
        return;

    // Transfer content layer -> mask layer (SrcIn)
    context->endLayer();
    // Transfer mask layer -> bg layer (SrcOver)
    context->endLayer();
}

void RenderSVGResourceClipper::drawMaskContent(GraphicsContext* context, const FloatRect& targetBoundingBox)
{
    ASSERT(frame());
    ASSERT(context);

    // Adjust the mask image context according to the target objectBoundingBox.
    AffineTransform maskContentTransformation;
    if (toSVGClipPathElement(element())->clipPathUnitsCurrentValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        maskContentTransformation.translate(targetBoundingBox.x(), targetBoundingBox.y());
        maskContentTransformation.scaleNonUniform(targetBoundingBox.width(), targetBoundingBox.height());
        context->concatCTM(maskContentTransformation);
    }

    // Switch to a paint behavior where all children of this <clipPath> will be rendered using special constraints:
    // - fill-opacity/stroke-opacity/opacity set to 1
    // - masker/filter not applied when rendering the children
    // - fill is set to the initial fill paint server (solid, black)
    // - stroke is set to the initial stroke paint server (none)
    PaintBehavior oldBehavior = frame()->view()->paintBehavior();
    frame()->view()->setPaintBehavior(oldBehavior | PaintBehaviorRenderingSVGMask);

    for (Node* childNode = element()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;

        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;

        WindRule newClipRule = style->svgStyle()->clipRule();
        bool isUseElement = childNode->hasTagName(SVGNames::useTag);
        if (isUseElement) {
            SVGUseElement* useElement = toSVGUseElement(childNode);
            renderer = useElement->rendererClipChild();
            if (!renderer)
                continue;
            if (!useElement->hasAttribute(SVGNames::clip_ruleAttr))
                newClipRule = renderer->style()->svgStyle()->clipRule();
        }

        // Only shapes, paths and texts are allowed for clipping.
        if (!renderer->isSVGShape() && !renderer->isSVGText())
            continue;

        context->setFillRule(newClipRule);

        if (isUseElement)
            renderer = childNode->renderer();

        SVGRenderingContext::renderSubtree(context, renderer, maskContentTransformation);
    }

    frame()->view()->setPaintBehavior(oldBehavior);
}

void RenderSVGResourceClipper::calculateClipContentRepaintRect()
{
    // This is a rough heuristic to appraise the clip size and doesn't consider clip on clip.
    for (Node* childNode = element()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        if (!renderer->isSVGShape() && !renderer->isSVGText() && !childNode->hasTagName(SVGNames::useTag))
            continue;
        RenderStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
             continue;
        m_clipBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->repaintRectInLocalCoordinates()));
    }
    m_clipBoundaries = toSVGClipPathElement(element())->animatedLocalTransform().mapRect(m_clipBoundaries);
}

bool RenderSVGResourceClipper::hitTestClipContent(const FloatRect& objectBoundingBox, const FloatPoint& nodeAtPoint)
{
    FloatPoint point = nodeAtPoint;
    if (!SVGRenderSupport::pointInClippingArea(this, point))
        return false;

    SVGClipPathElement* clipPathElement = toSVGClipPathElement(element());
    if (clipPathElement->clipPathUnitsCurrentValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        point = transform.inverse().mapPoint(point);
    }

    point = clipPathElement->animatedLocalTransform().inverse().mapPoint(point);

    for (Node* childNode = element()->firstChild(); childNode; childNode = childNode->nextSibling()) {
        RenderObject* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        if (!renderer->isSVGShape() && !renderer->isSVGText() && !childNode->hasTagName(SVGNames::useTag))
            continue;
        IntPoint hitPoint;
        HitTestResult result(hitPoint);
        if (renderer->nodeAtFloatPoint(HitTestRequest(HitTestRequest::SVGClipContent | HitTestRequest::DisallowShadowContent), result, point, HitTestForeground))
            return true;
    }

    return false;
}

FloatRect RenderSVGResourceClipper::resourceBoundingBox(RenderObject* object)
{
    // Resource was not layouted yet. Give back the boundingBox of the object.
    if (selfNeedsLayout())
        return object->objectBoundingBox();

    if (m_clipBoundaries.isEmpty())
        calculateClipContentRepaintRect();

    if (toSVGClipPathElement(element())->clipPathUnitsCurrentValue() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        FloatRect objectBoundingBox = object->objectBoundingBox();
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        return transform.mapRect(m_clipBoundaries);
    }

    return m_clipBoundaries;
}

}
