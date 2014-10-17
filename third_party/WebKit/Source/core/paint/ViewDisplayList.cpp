// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewDisplayList.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "platform/NotImplemented.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

void AtomicPaintChunk::replay(GraphicsContext* context)
{
    context->drawDisplayList(displayList.get());
}

void ClipDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    context->clip(clipRect);
    for (RoundedRect roundedRect : roundedRectClips)
        context->clipRoundedRect(roundedRect);
}

void EndClipDisplayItem::replay(GraphicsContext* context)
{
    context->restore();
}

PaintCommandRecorder::PaintCommandRecorder(GraphicsContext* context, RenderObject* renderer, PaintPhase phase, const FloatRect& clip)
    : m_context(context)
    , m_renderer(renderer)
    , m_phase(phase)
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        m_context->beginRecording(clip);
}

PaintCommandRecorder::~PaintCommandRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    OwnPtr<AtomicPaintChunk> paintChunk = adoptPtr(new AtomicPaintChunk(m_context->endRecording(), m_renderer, m_phase));
    ASSERT(m_renderer->view());
    m_renderer->view()->viewDisplayList().add(paintChunk.release());
}

ClipRecorder::ClipRecorder(RenderLayer* renderLayer, GraphicsContext* graphicsContext, ClipDisplayItem::ClipType clipType, const ClipRect& clipRect)
    : m_graphicsContext(graphicsContext)
    , m_renderLayer(renderLayer)
{
    IntRect snappedClipRect = pixelSnappedIntRect(clipRect.rect());
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        graphicsContext->save();
        graphicsContext->clip(snappedClipRect);
    } else {
        m_clipDisplayItem = adoptPtr(new ClipDisplayItem);
        m_clipDisplayItem->layer = renderLayer;
        m_clipDisplayItem->clipType = clipType;
        m_clipDisplayItem->clipRect = snappedClipRect;
    }
}

void ClipRecorder::addRoundedRectClip(const RoundedRect& roundedRect)
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        m_clipDisplayItem->roundedRectClips.append(roundedRect);
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

const PaintList& ViewDisplayList::paintList()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());

    updatePaintList();
    return m_newPaints;
}

void ViewDisplayList::add(WTF::PassOwnPtr<DisplayItem> displayItem)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_newPaints.append(displayItem);
}

void ViewDisplayList::invalidate(const RenderObject* renderer)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_invalidated.add(renderer);
}

bool ViewDisplayList::isRepaint(PaintList::iterator begin, const DisplayItem& displayItem)
{
    notImplemented();
    return false;
}

// Update the existing paintList by removing invalidated entries, updating repainted existing ones, and
// appending new items.
//
// The algorithm should be O(|existing paint list| + |newly painted list|). By using the ordering
// implied by the existing paint list, extra treewalks are avoided.
void ViewDisplayList::updatePaintList()
{
    notImplemented();
}

} // namespace blink
