// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/LayerClipRecorder.h"

#include "core/layout/ClipRect.h"
#include "core/layout/Layer.h"
#include "core/rendering/RenderView.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

LayerClipRecorder::LayerClipRecorder(const LayoutLayerModelObject* renderer, GraphicsContext* graphicsContext, DisplayItem::Type clipType, const ClipRect& clipRect,
    const LayerPaintingInfo* localPaintingInfo, const LayoutPoint& fragmentOffset, PaintLayerFlags paintFlags, BorderRadiusClippingRule rule)
    : m_graphicsContext(graphicsContext)
    , m_renderer(renderer)
    , m_clipType(clipType)
{
    ASSERT(renderer);

    IntRect snappedClipRect = pixelSnappedIntRect(clipRect.rect());
    OwnPtr<ClipDisplayItem> clipDisplayItem = ClipDisplayItem::create(renderer->displayItemClient(), clipType, snappedClipRect);
    if (localPaintingInfo && clipRect.hasRadius())
        collectRoundedRectClips(*renderer->layer(), *localPaintingInfo, graphicsContext, fragmentOffset, paintFlags, rule, clipDisplayItem->roundedRectClips());
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        clipDisplayItem->replay(graphicsContext);
    } else {
        ASSERT(m_graphicsContext->displayItemList());
        m_graphicsContext->displayItemList()->add(clipDisplayItem.release());
    }
}

static bool inContainingBlockChain(Layer* startLayer, Layer* endLayer)
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

void LayerClipRecorder::collectRoundedRectClips(Layer& renderLayer, const LayerPaintingInfo& localPaintingInfo, GraphicsContext* context, const LayoutPoint& fragmentOffset, PaintLayerFlags paintFlags,
    BorderRadiusClippingRule rule, Vector<FloatRoundedRect>& roundedRectClips)
{
    // If the clip rect has been tainted by a border radius, then we have to walk up our layer chain applying the clips from
    // any layers with overflow. The condition for being able to apply these clips is that the overflow object be in our
    // containing block chain so we check that also.
    for (Layer* layer = rule == IncludeSelfForBorderRadius ? &renderLayer : renderLayer.parent(); layer; layer = layer->parent()) {
        // Composited scrolling layers handle border-radius clip in the compositor via a mask layer. We do not
        // want to apply a border-radius clip to the layer contents itself, because that would require re-rastering
        // every frame to update the clip. We only want to make sure that the mask layer is properly clipped so
        // that it can in turn clip the scrolled contents in the compositor.
        if (layer->needsCompositedScrolling() && !(paintFlags & PaintLayerPaintingChildClippingMaskPhase))
            break;

        if (layer->renderer()->hasOverflowClip() && layer->renderer()->style()->hasBorderRadius() && inContainingBlockChain(&renderLayer, layer)) {
            LayoutPoint delta(fragmentOffset);
            layer->convertToLayerCoords(localPaintingInfo.rootLayer, delta);
            roundedRectClips.append(layer->renderer()->style()->getRoundedInnerBorderFor(LayoutRect(delta, LayoutSize(layer->size()))));
        }

        if (layer == localPaintingInfo.rootLayer)
            break;
    }
}

LayerClipRecorder::~LayerClipRecorder()
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        DisplayItem::Type endType = DisplayItem::clipTypeToEndClipType(m_clipType);
        OwnPtr<EndClipDisplayItem> endClip = EndClipDisplayItem::create(m_renderer->displayItemClient(), endType);
        ASSERT(m_graphicsContext->displayItemList());
        m_graphicsContext->displayItemList()->add(endClip.release());
    } else {
        m_graphicsContext->restore();
    }
}

} // namespace blink
