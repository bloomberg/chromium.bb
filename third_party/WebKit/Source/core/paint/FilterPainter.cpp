// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/FilterPainter.h"

#include "core/paint/LayerPainter.h"
#include "core/rendering/FilterEffectRenderer.h"
#include "core/rendering/RenderLayer.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/filters/FilterEffect.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"

namespace blink {

FilterPainter::FilterPainter(RenderLayer& renderLayer, GraphicsContext* context, const LayoutPoint& offsetFromRoot, const ClipRect& clipRect, LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags,
    LayoutRect& rootRelativeBounds, bool& rootRelativeBoundsComputed)
    : m_filterInProgress(false)
    , m_context(context)
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
        m_clipRecorder = adoptPtr(new ClipRecorder(renderLayer.renderer(), context, DisplayItem::ClipLayerFilter, clipRect));
        if (clipRect.hasRadius())
            LayerPainter::applyRoundedRectClips(renderLayer, paintingInfo, context, paintFlags, *m_clipRecorder);
    }

    context->save();
    FloatRect boundaries = mapImageFilterRect(imageFilter.get(), rootRelativeBounds);
    context->translate(rootRelativeBounds.x().toFloat(), rootRelativeBounds.y().toFloat());
    boundaries.move(-rootRelativeBounds.x().toFloat(), -rootRelativeBounds.y().toFloat());
    context->beginLayer(1, CompositeSourceOver, &boundaries, ColorFilterNone, imageFilter.get());
    context->translate(-rootRelativeBounds.x().toFloat(), -rootRelativeBounds.y().toFloat());
    m_filterInProgress = true;
}

FilterPainter::~FilterPainter()
{
    if (m_filterInProgress) {
        m_context->endLayer();
        m_context->restore();
    }
}

} // namespace blink
