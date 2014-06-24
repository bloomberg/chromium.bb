// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/compositing/CompositingInputsUpdater.h"

#include "core/rendering/RenderBlock.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "platform/TraceEvent.h"

namespace WebCore {

CompositingInputsUpdater::CompositingInputsUpdater(RenderLayer* rootRenderLayer)
    : m_geometryMap(UseTransforms)
    , m_rootRenderLayer(rootRenderLayer)
{
}

CompositingInputsUpdater::~CompositingInputsUpdater()
{
}

void CompositingInputsUpdater::update()
{
    TRACE_EVENT0("blink_rendering", "CompositingInputsUpdater::update");
    updateRecursive(m_rootRenderLayer, DoNotForceUpdate, AncestorInfo());
}

static const RenderLayer* findParentLayerOnContainingBlockChain(const RenderObject* object)
{
    for (const RenderObject* current = object; current; current = current->containingBlock()) {
        if (current->hasLayer())
            return static_cast<const RenderLayerModelObject*>(current)->layer();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void CompositingInputsUpdater::updateRecursive(RenderLayer* layer, UpdateType updateType, AncestorInfo info)
{
    if (!layer->childNeedsCompositingInputsUpdate() && updateType != ForceUpdate)
        return;

    m_geometryMap.pushMappingsToAncestor(layer, layer->parent());

    if (layer->hasCompositedLayerMapping())
        info.enclosingCompositedLayer = layer;

    if (layer->needsCompositingInputsUpdate()) {
        if (info.enclosingCompositedLayer)
            info.enclosingCompositedLayer->compositedLayerMapping()->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
        updateType = ForceUpdate;
    }

    if (updateType == ForceUpdate) {
        RenderLayer::CompositingInputs properties;

        if (!layer->isRootLayer()) {
            properties.clippedAbsoluteBoundingBox = enclosingIntRect(m_geometryMap.absoluteRect(layer->boundingBoxForCompositingOverlapTest()));
            // FIXME: Setting the absBounds to 1x1 instead of 0x0 makes very little sense,
            // but removing this code will make JSGameBench sad.
            // See https://codereview.chromium.org/13912020/
            if (properties.clippedAbsoluteBoundingBox.isEmpty())
                properties.clippedAbsoluteBoundingBox.setSize(IntSize(1, 1));

            IntRect clipRect = pixelSnappedIntRect(layer->clipper().backgroundClipRect(ClipRectsContext(m_rootRenderLayer, AbsoluteClipRects)).rect());
            properties.clippedAbsoluteBoundingBox.intersect(clipRect);

            const RenderLayer* parent = layer->parent();
            properties.opacityAncestor = parent->isTransparent() ? parent : parent->compositingInputs().opacityAncestor;
            properties.transformAncestor = parent->hasTransform() ? parent : parent->compositingInputs().transformAncestor;
            properties.filterAncestor = parent->hasFilter() ? parent : parent->compositingInputs().filterAncestor;

            if (info.lastScrollingAncestor) {
                const RenderObject* containingBlock = layer->renderer()->containingBlock();
                const RenderLayer* parentLayerOnContainingBlockChain = findParentLayerOnContainingBlockChain(containingBlock);

                properties.ancestorScrollingLayer = parentLayerOnContainingBlockChain->compositingInputs().ancestorScrollingLayer;
                if (parentLayerOnContainingBlockChain->scrollsOverflow())
                    properties.ancestorScrollingLayer = parentLayerOnContainingBlockChain;

                if (layer->renderer()->isOutOfFlowPositioned() && !layer->subtreeIsInvisible()) {
                    // FIXME: Why do we care about the lastScrollingAncestor in tree order? We're
                    // trying to tell CC that this layer isn't clipped by its apparent scrolling
                    // ancestor, but we present the tree to CC in stacking order rather than tree
                    // order. That would seem to imply that we'd be interested in the lastScrollingAncestor
                    // in stacking order.
                    const RenderObject* scroller = info.lastScrollingAncestor->renderer();
                    // FIXME: Why do we only walk up one step in the containing block chain?
                    // If there's a scroller between my containing block parent and my containing
                    // block grandparent, doesn't that make me an unclipped descendant?
                    properties.isUnclippedDescendant = scroller != containingBlock && scroller->isDescendantOf(containingBlock);
                }

                if (!layer->stackingNode()->isNormalFlowOnly()
                    && properties.ancestorScrollingLayer
                    && !info.ancestorStackingContext->renderer()->isDescendantOf(properties.ancestorScrollingLayer->renderer()))
                    properties.scrollParent = properties.ancestorScrollingLayer;
            }
        }

        layer->updateCompositingInputs(properties);
    }

    if (layer->stackingNode()->isStackingContext())
        info.ancestorStackingContext = layer;

    if (layer->scrollsOverflow())
        info.lastScrollingAncestor = layer;

    for (RenderLayer* child = layer->firstChild(); child; child = child->nextSibling())
        updateRecursive(child, updateType, info);

    m_geometryMap.popMappingsToAncestor(layer->parent());

    layer->clearChildNeedsCompositingInputsUpdate();
}

#if ASSERT_ENABLED

void CompositingInputsUpdater::assertNeedsCompositingInputsUpdateBitsCleared(RenderLayer* layer)
{
    ASSERT(!layer->childNeedsCompositingInputsUpdate());
    ASSERT(!layer->needsCompositingInputsUpdate());

    for (RenderLayer* child = layer->firstChild(); child; child = child->nextSibling())
        assertNeedsCompositingInputsUpdateBitsCleared(child);
}

#endif

} // namespace WebCore
