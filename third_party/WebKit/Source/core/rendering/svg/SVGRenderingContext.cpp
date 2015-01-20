/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "core/rendering/svg/SVGRenderingContext.h"

#include "core/frame/FrameHost.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/svg/RenderSVGResourceFilter.h"
#include "core/rendering/svg/RenderSVGResourceMasker.h"
#include "core/rendering/svg/SVGRenderSupport.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "platform/FloatConversion.h"

namespace blink {

SVGRenderingContext::~SVGRenderingContext()
{
    ASSERT(m_object && m_paintInfo);

    if (m_renderingFlags & PostApplyResources) {
        ASSERT(m_masker || m_clipper || m_filter);
        ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object));

        if (m_filter) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->filter() == m_filter);
            m_filter->finishEffect(m_object, m_paintInfo->context);
            m_paintInfo->rect = m_savedPaintRect;
            m_paintInfo->context->restore();
        }

        if (m_masker) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->masker() == m_masker);
            m_masker->finishEffect(m_object, m_paintInfo->context);
        }

        if (m_clipper) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->clipper() == m_clipper);
            m_clipper->postApplyStatefulResource(m_object, m_paintInfo->context, m_clipperState);
        }
    }
}

void SVGRenderingContext::prepareToRenderSVGContent(RenderObject* object, PaintInfo& paintInfo)
{
#if ENABLE(ASSERT)
    // This function must not be called twice!
    ASSERT(!(m_renderingFlags & PrepareToRenderSVGContentWasCalled));
    m_renderingFlags |= PrepareToRenderSVGContentWasCalled;
#endif

    ASSERT(object);
    m_object = object;
    m_paintInfo = &paintInfo;

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(m_object);

    // When rendering clip paths as masks, only geometric operations should be included so skip
    // non-geometric operations such as compositing, masking, and filtering.
    if (m_paintInfo->isRenderingClipPathAsMaskImage()) {
        if (!applyClipIfNecessary(resources))
            return;
        m_renderingFlags |= RenderingPrepared;
        return;
    }

    applyCompositingIfNecessary();

    if (!applyClipIfNecessary(resources))
        return;

    if (!applyMaskIfNecessary(resources))
        return;

    if (!applyFilterIfNecessary(resources))
        return;

    if (!isIsolationInstalled() && SVGRenderSupport::isIsolationRequired(m_object))
        m_compositingRecorder = adoptPtr(new CompositingRecorder(m_paintInfo->context, m_object->displayItemClient(), m_paintInfo->context->compositeOperation(), WebBlendModeNormal, 1, m_paintInfo->context->compositeOperation()));

    m_renderingFlags |= RenderingPrepared;
}

void SVGRenderingContext::applyCompositingIfNecessary()
{
    ASSERT(!m_paintInfo->isRenderingClipPathAsMaskImage());

    // RenderLayer takes care of root opacity and blend mode.
    if (m_object->isSVGRoot())
        return;

    RenderStyle* style = m_object->style();
    ASSERT(style);
    float opacity = style->opacity();
    bool hasBlendMode = style->hasBlendMode() && m_object->isBlendingAllowed();
    if (opacity < 1 || hasBlendMode) {
        m_clipRecorder = adoptPtr(new FloatClipRecorder(*m_paintInfo->context, m_object->displayItemClient(), m_paintInfo->phase, m_object->paintInvalidationRectInLocalCoordinates()));
        WebBlendMode blendMode = hasBlendMode ? style->blendMode() : WebBlendModeNormal;
        CompositeOperator compositeOp = hasBlendMode ? CompositeSourceOver : m_paintInfo->context->compositeOperation();
        m_compositingRecorder = adoptPtr(new CompositingRecorder(m_paintInfo->context, m_object->displayItemClient(), compositeOp, blendMode, opacity, compositeOp));
    }
}

bool SVGRenderingContext::applyClipIfNecessary(SVGResources* resources)
{
    // resources->clipper() corresponds to the non-prefixed 'clip-path' whereas
    // m_object->style()->clipPath() corresponds to '-webkit-clip-path'.
    // FIXME: We should unify the clip-path and -webkit-clip-path codepaths.
    if (RenderSVGResourceClipper* clipper = resources ? resources->clipper() : nullptr) {
        if (!clipper->applyStatefulResource(m_object, m_paintInfo->context, m_clipperState))
            return false;
        m_clipper = clipper;
        m_renderingFlags |= PostApplyResources;
    } else {
        ClipPathOperation* clipPathOperation = m_object->style()->clipPath();
        if (clipPathOperation && clipPathOperation->type() == ClipPathOperation::SHAPE) {
            ShapeClipPathOperation* clipPath = toShapeClipPathOperation(clipPathOperation);
            if (!clipPath->isValid())
                return false;
            m_clipPathRecorder = adoptPtr(new ClipPathRecorder(*m_paintInfo->context, m_object->displayItemClient(), clipPath->path(m_object->objectBoundingBox()), clipPath->windRule()));
        }
    }
    return true;
}

bool SVGRenderingContext::applyMaskIfNecessary(SVGResources* resources)
{
    if (RenderSVGResourceMasker* masker = resources ? resources->masker() : nullptr) {
        if (!masker->prepareEffect(m_object, m_paintInfo->context))
            return false;
        m_masker = masker;
        m_renderingFlags |= PostApplyResources;
    }
    return true;
}

bool SVGRenderingContext::applyFilterIfNecessary(SVGResources* resources)
{
    if (!resources) {
        if (m_object->style()->svgStyle().hasFilter())
            return false;
    } else if (RenderSVGResourceFilter* filter = resources->filter()) {
        // FIXME: This code should use the same pattern as clipping and masking
        // instead of always creating m_filter. See crbug.com/449743.
        m_filter = filter;
        m_renderingFlags |= PostApplyResources;
        m_savedPaintRect = m_paintInfo->rect;
        m_paintInfo->context->save();

        if (!filter->prepareEffect(m_object, m_paintInfo->context))
            return false;

        // Because we cache the filter contents and do not invalidate on paint
        // invalidation rect changes, we need to paint the entire filter region
        // so elements outside the initial paint (due to scrolling, etc) paint.
        m_paintInfo->rect = LayoutRect::infiniteIntRect();
    }
    return true;
}

bool SVGRenderingContext::isIsolationInstalled() const
{
    if (m_compositingRecorder)
        return true;
    if (m_masker || m_filter)
        return true;
    if (m_clipper && m_clipperState == RenderSVGResourceClipper::ClipperAppliedMask)
        return true;
    return false;
}

static AffineTransform& currentContentTransformation()
{
    DEFINE_STATIC_LOCAL(AffineTransform, s_currentContentTransformation, ());
    return s_currentContentTransformation;
}

SubtreeContentTransformScope::SubtreeContentTransformScope(const AffineTransform& subtreeContentTransformation)
{
    AffineTransform& contentTransformation = currentContentTransformation();
    m_savedContentTransformation = contentTransformation;
    contentTransformation = subtreeContentTransformation * contentTransformation;
}

SubtreeContentTransformScope::~SubtreeContentTransformScope()
{
    currentContentTransformation() = m_savedContentTransformation;
}

float SVGRenderingContext::calculateScreenFontSizeScalingFactor(const RenderObject* renderer)
{
    // FIXME: trying to compute a device space transform at record time is wrong. All clients
    // should be updated to avoid relying on this information, and the method should be removed.

    ASSERT(renderer);
    // We're about to possibly clear renderer, so save the deviceScaleFactor now.
    float deviceScaleFactor = renderer->document().frameHost()->deviceScaleFactor();

    // Walk up the render tree, accumulating SVG transforms.
    AffineTransform ctm = currentContentTransformation();
    while (renderer) {
        ctm = renderer->localToParentTransform() * ctm;
        if (renderer->isSVGRoot())
            break;
        renderer = renderer->parent();
    }

    // Continue walking up the layer tree, accumulating CSS transforms.
    // FIXME: this queries layer compositing state - which is not
    // supported during layout. Hence, the result may not include all CSS transforms.
    RenderLayer* layer = renderer ? renderer->enclosingLayer() : 0;
    while (layer && layer->isAllowedToQueryCompositingState()) {
        // We can stop at compositing layers, to match the backing resolution.
        // FIXME: should we be computing the transform to the nearest composited layer,
        // or the nearest composited layer that does not paint into its ancestor?
        // I think this is the nearest composited ancestor since we will inherit its
        // transforms in the composited layer tree.
        if (layer->compositingState() != NotComposited)
            break;

        if (TransformationMatrix* layerTransform = layer->transform())
            ctm = layerTransform->toAffineTransform() * ctm;

        layer = layer->parent();
    }

    ctm.scale(deviceScaleFactor);

    return narrowPrecisionToFloat(sqrt((pow(ctm.xScale(), 2) + pow(ctm.yScale(), 2)) / 2));
}

void SVGRenderingContext::renderSubtree(GraphicsContext* context, RenderObject* item)
{
    ASSERT(context);
    ASSERT(item);
    ASSERT(!item->needsLayout());

    PaintInfo info(context, LayoutRect::infiniteIntRect(), PaintPhaseForeground, PaintBehaviorNormal);
    item->paint(info, IntPoint());
}

} // namespace blink
