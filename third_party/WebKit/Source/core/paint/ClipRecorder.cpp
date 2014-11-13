// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ClipRecorder.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

ClipRecorder::ClipRecorder(RenderLayer* renderLayer, GraphicsContext* graphicsContext, DisplayItem::Type clipType, const ClipRect& clipRect)
    : m_graphicsContext(graphicsContext)
    , m_renderLayer(renderLayer)
{
    IntRect snappedClipRect = pixelSnappedIntRect(clipRect.rect());
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        graphicsContext->save();
        graphicsContext->clip(snappedClipRect);
    } else {
        m_clipDisplayItem = new ClipDisplayItem(nullptr, clipType, snappedClipRect);
#ifndef NDEBUG
        m_clipDisplayItem->setClientDebugString("nullptr");
#endif
        if (RenderLayer* container = m_renderLayer->enclosingLayerForPaintInvalidationCrossingFrameBoundaries())
            container->graphicsLayerBacking()->displayItemList().add(adoptPtr(m_clipDisplayItem));
    }
}

void ClipRecorder::addRoundedRectClip(const RoundedRect& roundedRect)
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        m_clipDisplayItem->roundedRectClips().append(roundedRect);
    else
        m_graphicsContext->clipRoundedRect(roundedRect);
}

ClipRecorder::~ClipRecorder()
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        OwnPtr<EndClipDisplayItem> endClip = adoptPtr(new EndClipDisplayItem);
        if (RenderLayer* container = m_renderLayer->enclosingLayerForPaintInvalidationCrossingFrameBoundaries())
            container->graphicsLayerBacking()->displayItemList().add(endClip.release());
    } else {
        m_graphicsContext->restore();
    }
}

} // namespace blink
