// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ClipRecorder.h"

#include "core/rendering/ClipRect.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntRect.h"
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

ClipRecorder::ClipRecorder(const RenderLayerModelObject* renderer, GraphicsContext* graphicsContext, DisplayItem::Type clipType, const ClipRect& clipRect,
    const LayerPaintingInfo* localPaintingInfo, const LayoutPoint& fragmentOffset, PaintLayerFlags paintFlags, BorderRadiusClippingRule rule)
    : m_graphicsContext(graphicsContext)
    , m_renderer(renderer)
{
    IntRect snappedClipRect = pixelSnappedIntRect(clipRect.rect());
    OwnPtr<ClipDisplayItem> clipDisplayItem = adoptPtr(new ClipDisplayItem(renderer, clipType, snappedClipRect));
    if (localPaintingInfo && clipRect.hasRadius())
        collectRoundedRectClips(*renderer->layer(), *localPaintingInfo, graphicsContext, fragmentOffset, paintFlags, rule, clipDisplayItem->roundedRectClips());
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        clipDisplayItem->replay(graphicsContext);
    } else {
        m_renderer->view()->viewDisplayList().add(clipDisplayItem.release());
    }
}

static bool inContainingBlockChain(RenderLayer* startLayer, RenderLayer* endLayer)
{
    if (startLayer == endLayer)
        return true;

    RenderView* view = startLayer->renderer()->view();
    for (RenderBlock* currentBlock = startLayer->renderer()->containingBlock(); currentBlock && currentBlock != view; currentBlock = currentBlock->containingBlock()) {
        if (currentBlock->layer() == endLayer)
            return true;
    }

    return false;
}

void ClipRecorder::collectRoundedRectClips(RenderLayer& renderLayer, const LayerPaintingInfo& localPaintingInfo, GraphicsContext* context, const LayoutPoint& fragmentOffset, PaintLayerFlags paintFlags,
    BorderRadiusClippingRule rule, Vector<RoundedRect>& roundedRectClips)
{
    // If the clip rect has been tainted by a border radius, then we have to walk up our layer chain applying the clips from
    // any layers with overflow. The condition for being able to apply these clips is that the overflow object be in our
    // containing block chain so we check that also.
    for (RenderLayer* layer = rule == IncludeSelfForBorderRadius ? &renderLayer : renderLayer.parent(); layer; layer = layer->parent()) {
        // Composited scrolling layers handle border-radius clip in the compositor via a mask layer. We do not
        // want to apply a border-radius clip to the layer contents itself, because that would require re-rastering
        // every frame to update the clip. We only want to make sure that the mask layer is properly clipped so
        // that it can in turn clip the scrolled contents in the compositor.
        if (layer->needsCompositedScrolling() && !(paintFlags & PaintLayerPaintingChildClippingMaskPhase))
            break;

        if (layer->renderer()->hasOverflowClip() && layer->renderer()->style()->hasBorderRadius() && inContainingBlockChain(&renderLayer, layer)) {
            LayoutPoint delta(fragmentOffset);
            layer->convertToLayerCoords(localPaintingInfo.rootLayer, delta);
            roundedRectClips.append(layer->renderer()->style()->getRoundedInnerBorderFor(LayoutRect(delta, layer->size())));
        }

        if (layer == localPaintingInfo.rootLayer)
            break;
    }
}

ClipRecorder::~ClipRecorder()
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        OwnPtr<EndClipDisplayItem> endClip = adoptPtr(new EndClipDisplayItem(m_renderer));
        m_renderer->view()->viewDisplayList().add(endClip.release());
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
