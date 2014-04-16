/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/compositing/GraphicsLayerUpdater.h"

#include "core/html/HTMLMediaElement.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerReflectionInfo.h"
#include "core/rendering/RenderPart.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "public/platform/Platform.h"

namespace WebCore {

static bool shouldAppendLayer(const RenderLayer& layer)
{
    if (!RuntimeEnabledFeatures::overlayFullscreenVideoEnabled())
        return true;
    Node* node = layer.renderer()->node();
    if (node && isHTMLMediaElement(*node) && toHTMLMediaElement(node)->isFullscreen())
        return false;
    return true;
}

GraphicsLayerUpdater::UpdateContext::UpdateContext(const UpdateContext& other, const RenderLayer& layer)
    : m_compositingStackingContainer(other.m_compositingStackingContainer)
    , m_compositingAncestor(other.m_compositingAncestor)
{
    CompositingState compositingState = layer.compositingState();
    if (compositingState != NotComposited && compositingState != PaintsIntoGroupedBacking) {
        m_compositingAncestor = &layer;
        if (layer.stackingNode()->isStackingContainer())
            m_compositingStackingContainer = &layer;
    }
}

const RenderLayer* GraphicsLayerUpdater::UpdateContext::compositingContainer(const RenderLayer& layer) const
{
    return layer.stackingNode()->isNormalFlowOnly() ? m_compositingAncestor : m_compositingStackingContainer;
}

GraphicsLayerUpdater::GraphicsLayerUpdater()
    : m_needsRebuildTree(false)
{
}

GraphicsLayerUpdater::~GraphicsLayerUpdater()
{
}

void GraphicsLayerUpdater::rebuildTree(RenderLayer& layer, GraphicsLayerVector& childLayersOfEnclosingLayer)
{
    // Make the layer compositing if necessary, and set up clipping and content layers.
    // Note that we can only do work here that is independent of whether the descendant layers
    // have been processed. computeCompositingRequirements() will already have done the repaint if necessary.

    layer.stackingNode()->updateLayerListsIfNeeded();

    const bool hasCompositedLayerMapping = layer.hasCompositedLayerMapping();
    CompositedLayerMappingPtr currentCompositedLayerMapping = layer.compositedLayerMapping();

    // If this layer has a compositedLayerMapping, then that is where we place subsequent children GraphicsLayers.
    // Otherwise children continue to append to the child list of the enclosing layer.
    GraphicsLayerVector layerChildren;
    GraphicsLayerVector& childList = hasCompositedLayerMapping ? layerChildren : childLayersOfEnclosingLayer;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer.stackingNode());
#endif

    if (layer.stackingNode()->isStackingContainer()) {
        RenderLayerStackingNodeIterator iterator(*layer.stackingNode(), NegativeZOrderChildren);
        while (RenderLayerStackingNode* curNode = iterator.next())
            rebuildTree(*curNode->layer(), childList);

        // If a negative z-order child is compositing, we get a foreground layer which needs to get parented.
        if (hasCompositedLayerMapping && currentCompositedLayerMapping->foregroundLayer())
            childList.append(currentCompositedLayerMapping->foregroundLayer());
    }

    RenderLayerStackingNodeIterator iterator(*layer.stackingNode(), NormalFlowChildren | PositiveZOrderChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        rebuildTree(*curNode->layer(), childList);

    if (hasCompositedLayerMapping) {
        bool parented = false;
        if (layer.renderer()->isRenderPart())
            parented = RenderLayerCompositor::parentFrameContentLayers(toRenderPart(layer.renderer()));

        if (!parented)
            currentCompositedLayerMapping->parentForSublayers()->setChildren(layerChildren);

        // If the layer has a clipping layer the overflow controls layers will be siblings of the clipping layer.
        // Otherwise, the overflow control layers are normal children.
        if (!currentCompositedLayerMapping->hasClippingLayer() && !currentCompositedLayerMapping->hasScrollingLayer()) {
            if (GraphicsLayer* overflowControlLayer = currentCompositedLayerMapping->layerForHorizontalScrollbar()) {
                overflowControlLayer->removeFromParent();
                currentCompositedLayerMapping->parentForSublayers()->addChild(overflowControlLayer);
            }

            if (GraphicsLayer* overflowControlLayer = currentCompositedLayerMapping->layerForVerticalScrollbar()) {
                overflowControlLayer->removeFromParent();
                currentCompositedLayerMapping->parentForSublayers()->addChild(overflowControlLayer);
            }

            if (GraphicsLayer* overflowControlLayer = currentCompositedLayerMapping->layerForScrollCorner()) {
                overflowControlLayer->removeFromParent();
                currentCompositedLayerMapping->parentForSublayers()->addChild(overflowControlLayer);
            }
        }

        if (shouldAppendLayer(layer))
            childLayersOfEnclosingLayer.append(currentCompositedLayerMapping->childForSuperlayers());
    }
}

void GraphicsLayerUpdater::update(RenderLayer& layer, UpdateType updateType, const UpdateContext& context)
{
    if (layer.hasCompositedLayerMapping()) {
        CompositedLayerMappingPtr mapping = layer.compositedLayerMapping();

        const RenderLayer* compositingContainer = context.compositingContainer(layer);
        ASSERT(compositingContainer == layer.ancestorCompositingLayer());
        if (mapping->updateRequiresOwnBackingStoreForAncestorReasons(compositingContainer))
            updateType = ForceUpdate;

        // Note carefully: here we assume that the compositing state of all descendants have been updated already,
        // so it is legitimate to compute and cache the composited bounds for this layer.
        mapping->updateCompositedBounds(updateType);

        if (RenderLayerReflectionInfo* reflection = layer.reflectionInfo()) {
            if (reflection->reflectionLayer()->hasCompositedLayerMapping())
                reflection->reflectionLayer()->compositedLayerMapping()->updateCompositedBounds(ForceUpdate);
        }

        if (mapping->updateGraphicsLayerConfiguration(updateType))
            m_needsRebuildTree = true;

        mapping->updateGraphicsLayerGeometry(updateType, compositingContainer);

        updateType = mapping->updateTypeForChildren(updateType);
        mapping->clearNeedsGraphicsLayerUpdate();

        if (!layer.parent())
            layer.compositor()->updateRootLayerPosition();

        if (mapping->hasUnpositionedOverflowControlsLayers())
            layer.scrollableArea()->positionOverflowControls();
    }

    UpdateContext childContext(context, layer);
    for (RenderLayer* child = layer.firstChild(); child; child = child->nextSibling())
        update(*child, updateType, childContext);
}

#if !ASSERT_DISABLED

void GraphicsLayerUpdater::assertNeedsToUpdateGraphicsLayerBitsCleared(RenderLayer& layer)
{
    if (layer.hasCompositedLayerMapping())
        layer.compositedLayerMapping()->assertNeedsToUpdateGraphicsLayerBitsCleared();

    for (RenderLayer* child = layer.firstChild(); child; child = child->nextSibling())
        assertNeedsToUpdateGraphicsLayerBitsCleared(*child);
}

#endif

} // namespace WebCore
