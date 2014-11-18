// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FilterPainter.h"

#include "core/paint/LayerPainter.h"
#include "core/rendering/FilterEffectRenderer.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"

namespace blink {

void BeginFilterDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    FloatRect boundaries = mapImageFilterRect(m_imageFilter.get(), m_bounds);
    context->translate(m_bounds.x().toFloat(), m_bounds.y().toFloat());
    boundaries.move(-m_bounds.x().toFloat(), -m_bounds.y().toFloat());
    context->beginLayer(1, CompositeSourceOver, &boundaries, ColorFilterNone, m_imageFilter.get());
    context->translate(-m_bounds.x().toFloat(), -m_bounds.y().toFloat());
}

#ifndef NDEBUG
WTF::String BeginFilterDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\", filter bounds: [%f,%f,%f,%f]}",
        rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data(),
        m_bounds.x().toFloat(), m_bounds.y().toFloat(), m_bounds.width().toFloat(), m_bounds.height().toFloat());
}
#endif

void EndFilterDisplayItem::replay(GraphicsContext* context)
{
    context->endLayer();
    context->restore();
}

#ifndef NDEBUG
WTF::String EndFilterDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\"}",
        rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data());
}
#endif

FilterPainter::FilterPainter(RenderLayer& renderLayer, GraphicsContext* context, const LayoutPoint& offsetFromRoot, const ClipRect& clipRect, LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags,
    LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed)
    : m_filterInProgress(false)
    , m_context(context)
    , m_renderer(renderLayer.renderer())
{
    if (!renderLayer.filterRenderer() || !renderLayer.paintsWithFilters())
        return;

    ASSERT(renderLayer.filterInfo());

    SkiaImageFilterBuilder builder(context);
    RefPtrWillBeRawPtr<FilterEffect> lastEffect = renderLayer.filterRenderer()->lastEffect();
    lastEffect->determineFilterPrimitiveSubregion(MapRectForward);
    RefPtr<ImageFilter> imageFilter = builder.build(lastEffect.get(), ColorSpaceDeviceRGB);
    if (!imageFilter)
        return;

    if (!rootRelativeBoundsComputed) {
        rootRelativeBounds = renderLayer.physicalBoundingBoxIncludingReflectionAndStackingChildren(paintingInfo.rootLayer, offsetFromRoot);
        rootRelativeBoundsComputed = true;
    }

    // We'll handle clipping to the dirty rect before filter rasterization.
    // Filter processing will automatically expand the clip rect and the offscreen to accommodate any filter outsets.
    // FIXME: It is incorrect to just clip to the damageRect here once multiple fragments are involved.

    // Subsequent code should not clip to the dirty rect, since we've already
    // done it above, and doing it later will defeat the outsets.
    paintingInfo.clipToDirtyRect = false;

    if (clipRect.rect() != paintingInfo.paintDirtyRect || clipRect.hasRadius()) {
        m_clipRecorder = adoptPtr(new ClipRecorder(renderLayer.renderer(), context, DisplayItem::ClipLayerFilter, clipRect, &paintingInfo, LayoutPoint(), paintFlags));
    }

    OwnPtr<BeginFilterDisplayItem> filterDisplayItem = adoptPtr(new BeginFilterDisplayItem(m_renderer, DisplayItem::BeginFilter, imageFilter, rootRelativeBounds));
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        renderLayer.renderer()->view()->viewDisplayList().add(filterDisplayItem.release());
    } else {
        filterDisplayItem->replay(context);
    }

    m_filterInProgress = true;
}

FilterPainter::~FilterPainter()
{
    if (!m_filterInProgress)
        return;

    OwnPtr<EndFilterDisplayItem> endFilterDisplayItem = adoptPtr(new EndFilterDisplayItem(m_renderer));
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        m_renderer->view()->viewDisplayList().add(endFilterDisplayItem.release());
    } else {
        endFilterDisplayItem->replay(m_context);
    }
}

} // namespace blink
