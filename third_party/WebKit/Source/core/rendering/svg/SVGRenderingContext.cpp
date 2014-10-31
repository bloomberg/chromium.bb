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
    // Fast path if we don't need to restore anything.
    if (!(m_renderingFlags & ActionsNeeded))
        return;

    ASSERT(m_object && m_paintInfo);

    if (m_renderingFlags & PostApplyResources) {
        ASSERT(m_masker || m_clipper || m_filter);
        ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object));

        if (m_filter) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->filter() == m_filter);
            m_filter->finishEffect(m_object, m_paintInfo->context);
            m_paintInfo->context = m_savedContext;
            m_paintInfo->rect = m_savedPaintRect;
        }

        if (m_clipper) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->clipper() == m_clipper);
            m_clipper->postApplyStatefulResource(m_object, m_paintInfo->context, m_clipperState);
        }

        if (m_masker) {
            ASSERT(SVGResourcesCache::cachedResourcesForRenderObject(m_object)->masker() == m_masker);
            m_masker->finishEffect(m_object, m_paintInfo->context);
        }
    }

    if (m_renderingFlags & EndOpacityLayer)
        m_paintInfo->context->endLayer();

    if (m_renderingFlags & RestoreGraphicsContext)
        m_paintInfo->context->restore();
}

void SVGRenderingContext::prepareToRenderSVGContent(RenderObject* object, PaintInfo& paintInfo)
{
    ASSERT(object);

#if ENABLE(ASSERT)
    // This function must not be called twice!
    ASSERT(!(m_renderingFlags & PrepareToRenderSVGContentWasCalled));
    m_renderingFlags |= PrepareToRenderSVGContentWasCalled;
#endif

    m_object = object;
    m_paintInfo = &paintInfo;

    RenderStyle* style = m_object->style();
    ASSERT(style);

    const SVGRenderStyle& svgStyle = style->svgStyle();

    // Setup transparency layers before setting up SVG resources!
    bool isRenderingMask = SVGRenderSupport::isRenderingClipPathAsMaskImage(*m_object);
    // RenderLayer takes care of root opacity.
    float opacity = object->isSVGRoot() ? 1 : style->opacity();
    bool hasBlendMode = style->hasBlendMode();

    if (!isRenderingMask && (opacity < 1 || hasBlendMode || style->hasIsolation())) {
        FloatRect paintInvalidationRect = m_object->paintInvalidationRectInLocalCoordinates();
        m_paintInfo->context->clip(paintInvalidationRect);

        if (hasBlendMode) {
            if (!(m_renderingFlags & RestoreGraphicsContext)) {
                m_paintInfo->context->save();
                m_renderingFlags |= RestoreGraphicsContext;
            }
            m_paintInfo->context->setCompositeOperation(CompositeSourceOver, style->blendMode());
        }

        m_paintInfo->context->beginTransparencyLayer(opacity);

        if (hasBlendMode)
            m_paintInfo->context->setCompositeOperation(CompositeSourceOver, WebBlendModeNormal);

        m_renderingFlags |= EndOpacityLayer;
    }

    SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(m_object);

    // Prefer a 'clipper' (non-prefixed 'clip-path') to a 'clip shape'
    // ('-webkit-clip-path'), until these two properties end up being merged
    // properly.
    if (RenderSVGResourceClipper* clipper = resources ? resources->clipper() : nullptr) {
        if (!clipper->applyStatefulResource(m_object, m_paintInfo->context, m_clipperState))
            return;
        m_clipper = clipper;
        m_renderingFlags |= PostApplyResources;
    } else {
        ClipPathOperation* clipPathOperation = style->clipPath();
        if (clipPathOperation && clipPathOperation->type() == ClipPathOperation::SHAPE) {
            ShapeClipPathOperation* clipPath = toShapeClipPathOperation(clipPathOperation);
            m_paintInfo->context->clipPath(clipPath->path(object->objectBoundingBox()), clipPath->windRule());
        }
    }

    if (isRenderingMask) {
        m_renderingFlags |= RenderingPrepared;
        return;
    }

    if (resources) {
        if (RenderSVGResourceMasker* masker = resources->masker()) {
            if (!masker->prepareEffect(m_object, m_paintInfo->context))
                return;
            m_masker = masker;
            m_renderingFlags |= PostApplyResources;
        }

        m_filter = resources->filter();
        if (m_filter) {
            m_savedContext = m_paintInfo->context;
            m_savedPaintRect = m_paintInfo->rect;
            // Return with false here may mean that we don't need to draw the content
            // (because it was either drawn before or empty) but we still need to apply the filter.
            m_renderingFlags |= PostApplyResources;
            if (!m_filter->prepareEffect(m_object, m_paintInfo->context))
                return;

            // Since we're caching the resulting bitmap and do not invalidate it on paint invalidation rect
            // changes, we need to paint the whole filter region. Otherwise, elements not visible
            // at the time of the initial paint (due to scrolling, window size, etc.) will never
            // be drawn.
            m_paintInfo->rect = IntRect(m_filter->drawingRegion(m_object));
        }
    } else {
        // Broken filter disables rendering.
        if (svgStyle.hasFilter())
            return;
    }

    m_renderingFlags |= RenderingPrepared;
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

    PaintInfo info(context, PaintInfo::infiniteRect(), PaintPhaseForeground, PaintBehaviorNormal);
    item->paint(info, IntPoint());
}

} // namespace blink
