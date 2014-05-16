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
#include "core/rendering/compositing/CompositingLayerAssigner.h"

#include "core/rendering/compositing/CompositedLayerMapping.h"

namespace WebCore {

// We will only allow squashing if the bbox-area:squashed-area doesn't exceed
// the ratio |gSquashingSparsityTolerance|:1.
static uint64_t gSquashingSparsityTolerance = 6;

CompositingLayerAssigner::CompositingLayerAssigner(RenderLayerCompositor* compositor)
    : m_compositor(compositor)
    , m_layerSquashingEnabled(compositor->layerSquashingEnabled())
{
}

CompositingLayerAssigner::~CompositingLayerAssigner()
{
}

void CompositingLayerAssigner::assign(RenderLayer* updateRoot, bool& layersChanged)
{
    SquashingState squashingState;
    assignLayersToBackingsInternal(updateRoot, squashingState, layersChanged);
    if (squashingState.hasMostRecentMapping)
        squashingState.mostRecentMapping->finishAccumulatingSquashingLayers(squashingState.nextSquashedLayerIndex);
}

void CompositingLayerAssigner::SquashingState::updateSquashingStateForNewMapping(CompositedLayerMappingPtr newCompositedLayerMapping, bool hasNewCompositedLayerMapping, LayoutPoint newOffsetFromTransformedAncestorForSquashingCLM)
{
    // The most recent backing is done accumulating any more squashing layers.
    if (hasMostRecentMapping)
        mostRecentMapping->finishAccumulatingSquashingLayers(nextSquashedLayerIndex);

    nextSquashedLayerIndex = 0;
    boundingRect = IntRect();
    mostRecentMapping = newCompositedLayerMapping;
    hasMostRecentMapping = hasNewCompositedLayerMapping;
    offsetFromTransformedAncestorForSquashingCLM = newOffsetFromTransformedAncestorForSquashingCLM;
}

bool CompositingLayerAssigner::squashingWouldExceedSparsityTolerance(const RenderLayer* candidate, const CompositingLayerAssigner::SquashingState& squashingState)
{
    IntRect bounds = candidate->ancestorDependentProperties().clippedAbsoluteBoundingBox;
    IntRect newBoundingRect = squashingState.boundingRect;
    newBoundingRect.unite(bounds);
    const uint64_t newBoundingRectArea = newBoundingRect.size().area();
    const uint64_t newSquashedArea = squashingState.totalAreaOfSquashedRects + bounds.size().area();
    return newBoundingRectArea > gSquashingSparsityTolerance * newSquashedArea;
}

bool CompositingLayerAssigner::needsOwnBacking(const RenderLayer* layer) const
{
    if (!m_compositor->canBeComposited(layer))
        return false;

    // If squashing is disabled, then layers that would have been squashed should just be separately composited.
    bool needsOwnBackingForDisabledSquashing = !m_layerSquashingEnabled && requiresSquashing(layer->compositingReasons());

    return requiresCompositing(layer->compositingReasons()) || needsOwnBackingForDisabledSquashing || (m_compositor->staleInCompositingMode() && layer->isRootLayer());
}

CompositingStateTransitionType CompositingLayerAssigner::computeCompositedLayerUpdate(RenderLayer* layer)
{
    CompositingStateTransitionType update = NoCompositingStateChange;
    if (needsOwnBacking(layer)) {
        if (!layer->hasCompositedLayerMapping()) {
            update = AllocateOwnCompositedLayerMapping;
        }
    } else {
        if (layer->hasCompositedLayerMapping())
            update = RemoveOwnCompositedLayerMapping;

        if (m_layerSquashingEnabled) {
            if (!layer->subtreeIsInvisible() && requiresSquashing(layer->compositingReasons())) {
                // We can't compute at this time whether the squashing layer update is a no-op,
                // since that requires walking the render layer tree.
                update = PutInSquashingLayer;
            } else if (layer->groupedMapping() || layer->lostGroupedMapping()) {
                update = RemoveFromSquashingLayer;
            }
        }
    }
    return update;
}

bool CompositingLayerAssigner::canSquashIntoCurrentSquashingOwner(const RenderLayer* layer, const CompositingLayerAssigner::SquashingState& squashingState)
{
    // FIXME: this special case for video exists only to deal with corner cases
    // where a RenderVideo does not report that it needs to be directly composited.
    // Video does not currently support sharing a backing, but this could be
    // generalized in the future. The following layout tests fail if we permit the
    // video to share a backing with other layers.
    //
    // compositing/video/video-controls-layer-creation.html
    // virtual/softwarecompositing/video/video-controls-layer-creation.html
    if (layer->renderer()->isVideo())
        return false;

    if (squashingWouldExceedSparsityTolerance(layer, squashingState))
        return false;

    // FIXME: this is not efficient, since it walks up the tree . We should store these values on the AncestorDependentPropertiesCache.
    ASSERT(squashingState.hasMostRecentMapping);
    const RenderLayer& squashingLayer = squashingState.mostRecentMapping->owningLayer();

    if (layer->renderer()->clippingContainer() != squashingLayer.renderer()->clippingContainer()) {
        if (!squashingLayer.compositedLayerMapping()->containingSquashedLayer(layer->renderer()->clippingContainer()))
            return false;
    }

    if (layer->compositingContainer() == &squashingLayer)
        return false;

    // Composited descendants need to be clipped by a child contianment graphics layer, which would not be available if the layer is
    if (m_compositor->clipsCompositingDescendants(layer))
        return false;

    if (layer->scrollsWithRespectTo(&squashingLayer))
        return false;

    const RenderLayer::AncestorDependentProperties& ancestorDependentProperties = layer->ancestorDependentProperties();
    const RenderLayer::AncestorDependentProperties& squashingLayerAncestorDependentProperties = squashingLayer.ancestorDependentProperties();

    if (ancestorDependentProperties.opacityAncestor != squashingLayerAncestorDependentProperties.opacityAncestor)
        return false;

    if (ancestorDependentProperties.transformAncestor != squashingLayerAncestorDependentProperties.transformAncestor)
        return false;

    if (ancestorDependentProperties.filterAncestor != squashingLayerAncestorDependentProperties.filterAncestor)
        return false;

    return true;
}

bool CompositingLayerAssigner::updateSquashingAssignment(RenderLayer* layer, SquashingState& squashingState, const CompositingStateTransitionType compositedLayerUpdate)
{
    // NOTE: In the future as we generalize this, the background of this layer may need to be assigned to a different backing than
    // the squashed RenderLayer's own primary contents. This would happen when we have a composited negative z-index element that needs
    // to paint on top of the background, but below the layer's main contents. For now, because we always composite layers
    // when they have a composited negative z-index child, such layers will never need squashing so it is not yet an issue.
    if (compositedLayerUpdate == PutInSquashingLayer) {
        // A layer that is squashed with other layers cannot have its own CompositedLayerMapping.
        ASSERT(!layer->hasCompositedLayerMapping());
        ASSERT(squashingState.hasMostRecentMapping);

        LayoutPoint offsetFromTransformedAncestorForSquashedLayer = layer->computeOffsetFromTransformedAncestor();

        // Compute the offset of this layer from the squashing owner. This computation is correct only because layers are allowed to squash only if they
        // share a transformed ancestor (see canSquashIntoCurrentSquashingOwner).
        LayoutSize offsetFromSquashingCLM(offsetFromTransformedAncestorForSquashedLayer.x() - squashingState.offsetFromTransformedAncestorForSquashingCLM.x(),
            offsetFromTransformedAncestorForSquashedLayer.y() - squashingState.offsetFromTransformedAncestorForSquashingCLM.y());

        bool changedSquashingLayer =
            squashingState.mostRecentMapping->updateSquashingLayerAssignment(layer, offsetFromSquashingCLM, squashingState.nextSquashedLayerIndex);
        if (!changedSquashingLayer)
            return true;

        // If we've modified the collection of squashed layers, we must update
        // the graphics layer geometry.
        squashingState.mostRecentMapping->setNeedsGraphicsLayerUpdate();

        layer->clipper().clearClipRectsIncludingDescendants();

        // If we need to repaint, do so before allocating the layer to the squashing layer.
        m_compositor->repaintOnCompositingChange(layer);

        // FIXME: it seems premature to compute this before all compositing state has been updated?
        // This layer and all of its descendants have cached repaints rects that are relative to
        // the repaint container, so change when compositing changes; we need to update them here.

        // FIXME: what's up with parent()?
        if (layer->parent())
            layer->repainter().computeRepaintRectsIncludingDescendants();

        return true;
    }
    if (compositedLayerUpdate == RemoveFromSquashingLayer) {
        if (layer->groupedMapping()) {
            layer->groupedMapping()->setNeedsGraphicsLayerUpdate();
            layer->setGroupedMapping(0);
        }

        // This layer and all of its descendants have cached repaints rects that are relative to
        // the repaint container, so change when compositing changes; we need to update them here.
        layer->repainter().computeRepaintRectsIncludingDescendants();

        // If we need to repaint, do so now that we've removed it from a squashed layer
        m_compositor->repaintOnCompositingChange(layer);

        layer->setLostGroupedMapping(false);
        return true;
    }

    return false;
}

void CompositingLayerAssigner::assignLayersToBackingsForReflectionLayer(RenderLayer* reflectionLayer, bool& layersChanged)
{
    CompositingStateTransitionType compositedLayerUpdate = computeCompositedLayerUpdate(reflectionLayer);
    if (compositedLayerUpdate != NoCompositingStateChange) {
        layersChanged = true;
        m_compositor->allocateOrClearCompositedLayerMapping(reflectionLayer, compositedLayerUpdate);
    }
    m_compositor->updateDirectCompositingReasons(reflectionLayer);
    if (reflectionLayer->hasCompositedLayerMapping())
        reflectionLayer->compositedLayerMapping()->updateGraphicsLayerConfiguration(GraphicsLayerUpdater::ForceUpdate);
}

void CompositingLayerAssigner::assignLayersToBackingsInternal(RenderLayer* layer, SquashingState& squashingState, bool& layersChanged)
{
    if (m_layerSquashingEnabled && requiresSquashing(layer->compositingReasons()) && !canSquashIntoCurrentSquashingOwner(layer, squashingState))
        layer->setCompositingReasons(layer->compositingReasons() | CompositingReasonNoSquashingTargetFound);

    CompositingStateTransitionType compositedLayerUpdate = computeCompositedLayerUpdate(layer);

    if (m_compositor->allocateOrClearCompositedLayerMapping(layer, compositedLayerUpdate))
        layersChanged = true;

    // FIXME: special-casing reflection layers here is not right.
    if (layer->reflectionInfo())
        assignLayersToBackingsForReflectionLayer(layer->reflectionInfo()->reflectionLayer(), layersChanged);

    // Add this layer to a squashing backing if needed.
    if (m_layerSquashingEnabled) {
        if (updateSquashingAssignment(layer, squashingState, compositedLayerUpdate))
            layersChanged = true;

        const bool layerIsSquashed = compositedLayerUpdate == PutInSquashingLayer || (compositedLayerUpdate == NoCompositingStateChange && layer->groupedMapping());
        if (layerIsSquashed) {
            squashingState.nextSquashedLayerIndex++;
            IntRect layerBounds = layer->ancestorDependentProperties().clippedAbsoluteBoundingBox;
            squashingState.totalAreaOfSquashedRects += layerBounds.size().area();
            squashingState.boundingRect.unite(layerBounds);
        }
    }

    if (layer->stackingNode()->isStackingContainer()) {
        RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), NegativeZOrderChildren);
        while (RenderLayerStackingNode* curNode = iterator.next())
            assignLayersToBackingsInternal(curNode->layer(), squashingState, layersChanged);
    }

    if (m_layerSquashingEnabled) {
        // At this point, if the layer is to be "separately" composited, then its backing becomes the most recent in paint-order.
        if (layer->compositingState() == PaintsIntoOwnBacking || layer->compositingState() == HasOwnBackingButPaintsIntoAncestor) {
            ASSERT(!requiresSquashing(layer->compositingReasons()));
            LayoutPoint offsetFromTransformedAncestorForSquashingCLM = layer->computeOffsetFromTransformedAncestor();
            squashingState.updateSquashingStateForNewMapping(layer->compositedLayerMapping(), layer->hasCompositedLayerMapping(), offsetFromTransformedAncestorForSquashingCLM);
        }
    }

    RenderLayerStackingNodeIterator iterator(*layer->stackingNode(), NormalFlowChildren | PositiveZOrderChildren);
    while (RenderLayerStackingNode* curNode = iterator.next())
        assignLayersToBackingsInternal(curNode->layer(), squashingState, layersChanged);
}

}
