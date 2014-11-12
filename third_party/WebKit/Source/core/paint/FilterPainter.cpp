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

FilterPainter::FilterPainter(RenderLayer& renderLayer, GraphicsContext* context, const FloatRect& filterBoxRect, const ClipRect& clipRect, const LayerPaintingInfo& paintingInfo, PaintLayerFlags paintFlags)
    : m_filterInProgress(false)
    , m_context(context)
{
    SkiaImageFilterBuilder builder(context);
    RefPtrWillBeRawPtr<FilterEffect> lastEffect = renderLayer.filterRenderer()->lastEffect();
    lastEffect->determineFilterPrimitiveSubregion(MapRectForward);
    RefPtr<ImageFilter> imageFilter = builder.build(lastEffect.get(), ColorSpaceDeviceRGB);
    if (!imageFilter)
        return;

    if (clipRect.rect() != paintingInfo.paintDirtyRect || clipRect.hasRadius()) {
        m_clipRecorder = adoptPtr(new ClipRecorder(&renderLayer, context, DisplayItem::ClipLayerFilter, clipRect));
        if (clipRect.hasRadius())
            LayerPainter::applyRoundedRectClips(renderLayer, paintingInfo, context, paintFlags, *m_clipRecorder);
    }

    context->save();
    FloatRect boundaries = mapImageFilterRect(imageFilter.get(), filterBoxRect);
    context->translate(filterBoxRect.x(), filterBoxRect.y());
    boundaries.move(-filterBoxRect.x(), -filterBoxRect.y());
    context->beginLayer(1, CompositeSourceOver, &boundaries, ColorFilterNone, imageFilter.get());
    context->translate(-filterBoxRect.x(), -filterBoxRect.y());
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
