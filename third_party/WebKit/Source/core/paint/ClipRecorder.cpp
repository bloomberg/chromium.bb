// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ClipRecorder.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

void ClipDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    context->clip(m_clipRect);
    for (RoundedRect roundedRect : m_roundedRectClips)
        context->clipRoundedRect(roundedRect);
}

void EndClipDisplayItem::replay(GraphicsContext* context)
{
    context->restore();
}

ClipRecorder::ClipRecorder(RenderLayer* renderLayer, GraphicsContext* graphicsContext, DisplayItem::Type clipType, const ClipRect& clipRect)
    : m_graphicsContext(graphicsContext)
    , m_renderLayer(renderLayer)
{
    IntRect snappedClipRect = pixelSnappedIntRect(clipRect.rect());
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        graphicsContext->save();
        graphicsContext->clip(snappedClipRect);
    } else {
        m_clipDisplayItem = new ClipDisplayItem(0, renderLayer, clipType, snappedClipRect);
        m_renderLayer->renderer()->view()->viewDisplayList().add(adoptPtr(m_clipDisplayItem));
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
        m_renderLayer->renderer()->view()->viewDisplayList().add(endClip.release());
    } else {
        m_graphicsContext->restore();
    }
}

#ifndef NDEBUG
WTF::String ClipDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\", clipRect: [%d,%d,%d,%d]}",
        rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data(),
        m_clipRect.x(), m_clipRect.y(), m_clipRect.width(), m_clipRect.height());
}
#endif

} // namespace blink
