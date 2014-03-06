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

bool shouldAppendLayer(const RenderLayer& layer)
{
    if (!RuntimeEnabledFeatures::overlayFullscreenVideoEnabled())
        return true;
    Node* node = layer.renderer()->node();
    if (isHTMLMediaElement(*node) && toHTMLMediaElement(node)->isFullscreen())
        return false;
    return true;
}

GraphicsLayerUpdater::GraphicsLayerUpdater(RenderView& renderView)
    : m_renderView(renderView)
    , m_pixelsWithoutPromotingAllTransitions(0.0)
    , m_pixelsAddedByPromotingAllTransitions(0.0)
{
}

GraphicsLayerUpdater::~GraphicsLayerUpdater()
{
}

void GraphicsLayerUpdater::rebuildTree(RenderLayer& layer, Vector<GraphicsLayer*>& childLayersOfEnclosingLayer, int depth)
{
    // Make the layer compositing if necessary, and set up clipping and content layers.
    // Note that we can only do work here that is independent of whether the descendant layers
    // have been processed. computeCompositingRequirements() will already have done the repaint if necessary.

    layer.stackingNode()->updateLayerListsIfNeeded();
    layer.update3dRenderingContext();

    const bool hasCompositedLayerMapping = layer.hasCompositedLayerMapping();
    CompositedLayerMappingPtr currentCompositedLayerMapping = layer.compositedLayerMapping();

    update(layer);

    // Grab some stats for histograms.
    if (hasCompositedLayerMapping) {
        m_pixelsWithoutPromotingAllTransitions += layer.size().height() * layer.size().width();
    } else {
        if ((layer.renderer()->style()->transitionForProperty(CSSPropertyOpacity) ||
             layer.renderer()->style()->transitionForProperty(CSSPropertyWebkitTransform)) &&
            m_renderView.viewRect().intersects(layer.absoluteBoundingBox()))
            m_pixelsAddedByPromotingAllTransitions += layer.size().height() * layer.size().width();
    }

    // If this layer has a compositedLayerMapping, then that is where we place subsequent children GraphicsLayers.
    // Otherwise children continue to append to the child list of the enclosing layer.
    Vector<GraphicsLayer*> layerChildren;
    Vector<GraphicsLayer*>& childList = hasCompositedLayerMapping ? layerChildren : childLayersOfEnclosingLayer;

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer.stackingNode());
#endif

    if (layer.stackingNode()->isStackingContainer()) {
        RenderLayerStackingNodeIterator iterator(*layer.stackingNode(), NegativeZOrderChildren);
        while (RenderLayerStackingNode* curNode = iterator.next())
            rebuildTree(*curNode->layer(), childList, depth + 1);

        // If a negative z-order child is compositing, we get a foreground layer which needs to get parented.
        if (hasCompositedLayerMapping && currentCompositedLayerMapping->foregroundLayer())
            childList.append(currentCompositedLayerMapping->foregroundLayer());
    }

    RenderLayerStackingNodeIterator iterator(*layer.stackingNode(), NormalFlowChildren | PositiveZOrderChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        rebuildTree(*curNode->layer(), childList, depth + 1);

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

    if (!depth) {
        int percentageIncreaseInPixels = static_cast<int>(m_pixelsAddedByPromotingAllTransitions / m_pixelsWithoutPromotingAllTransitions * 100);
        blink::Platform::current()->histogramCustomCounts("Renderer.PixelIncreaseFromTransitions", percentageIncreaseInPixels, 0, 1000, 50);
    }
}

// This just updates layer geometry without changing the hierarchy.
void GraphicsLayerUpdater::updateRecursive(RenderLayer& layer)
{
    update(layer);

#if !ASSERT_DISABLED
    LayerListMutationDetector mutationChecker(layer.stackingNode());
#endif

    RenderLayerStackingNodeIterator iterator(*layer.stackingNode(), AllChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        updateRecursive(*curNode->layer());
}

void GraphicsLayerUpdater::update(RenderLayer& layer)
{
    if (!layer.hasCompositedLayerMapping())
        return;

    CompositedLayerMappingPtr mapping = layer.compositedLayerMapping();

    // Note carefully: here we assume that the compositing state of all descendants have been updated already,
    // so it is legitimate to compute and cache the composited bounds for this layer.
    mapping->updateCompositedBounds();

    if (RenderLayerReflectionInfo* reflection = layer.reflectionInfo()) {
        if (reflection->reflectionLayer()->hasCompositedLayerMapping())
            reflection->reflectionLayer()->compositedLayerMapping()->updateCompositedBounds();
    }

    mapping->updateGraphicsLayerConfiguration();
    mapping->updateGraphicsLayerGeometry();

    if (!layer.parent())
        layer.compositor()->updateRootLayerPosition();

    if (mapping->hasUnpositionedOverflowControlsLayers())
        layer.scrollableArea()->positionOverflowControls();
}

} // namespace WebCore
