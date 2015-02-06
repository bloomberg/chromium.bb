// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/compositing/CompositingInputsUpdater.h"

#include "core/layout/Layer.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/LayerCompositor.h"
#include "core/rendering/RenderBlock.h"
#include "platform/TraceEvent.h"

namespace blink {

CompositingInputsUpdater::CompositingInputsUpdater(Layer* rootLayer)
    : m_geometryMap(UseTransforms)
    , m_rootLayer(rootLayer)
{
}

CompositingInputsUpdater::~CompositingInputsUpdater()
{
}

void CompositingInputsUpdater::update()
{
    TRACE_EVENT0("blink", "CompositingInputsUpdater::update");
    updateRecursive(m_rootLayer, DoNotForceUpdate, AncestorInfo());
}

static const Layer* findParentLayerOnClippingContainerChain(const Layer* layer)
{
    LayoutObject* current = layer->renderer();
    while (current) {
        if (current->style()->position() == FixedPosition) {
            for (current = current->parent(); current && !current->canContainFixedPositionObjects(); current = current->parent()) {
                // All types of clips apply to fixed-position descendants of other fixed-position elements.
                // Note: it's unclear whether this is what the spec says. Firefox does not clip, but Chrome does.
                if (current->style()->position() == FixedPosition && current->hasClipOrOverflowClip()) {
                    ASSERT(current->hasLayer());
                    return static_cast<const LayoutLayerModelObject*>(current)->layer();
                }

                // CSS clip applies to fixed position elements even for ancestors that are not what the
                // fixed element is positioned with respect to.
                if (current->hasClip()) {
                    ASSERT(current->hasLayer());
                    return static_cast<const LayoutLayerModelObject*>(current)->layer();
                }
            }
        } else {
            current = current->containingBlock();
        }

        if (current->hasLayer())
            return static_cast<const LayoutLayerModelObject*>(current)->layer();
        // Having clip or overflow clip forces the LayoutObject to become a layer.
        ASSERT(!current->hasClipOrOverflowClip());
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static const Layer* findParentLayerOnContainingBlockChain(const LayoutObject* object)
{
    for (const LayoutObject* current = object; current; current = current->containingBlock()) {
        if (current->hasLayer())
            return static_cast<const LayoutLayerModelObject*>(current)->layer();
    }
    ASSERT_NOT_REACHED();
    return 0;
}

static bool hasClippedStackingAncestor(const Layer* layer, const Layer* clippingLayer)
{
    if (layer == clippingLayer)
        return false;
    const LayoutObject* clippingRenderer = clippingLayer->renderer();
    for (const Layer* current = layer->compositingContainer(); current && current != clippingLayer; current = current->compositingContainer()) {
        if (current->renderer()->hasClipOrOverflowClip() && !clippingRenderer->isDescendantOf(current->renderer()))
            return true;

        if (const LayoutObject* container = current->clippingContainer()) {
            if (clippingRenderer != container && !clippingRenderer->isDescendantOf(container))
                return true;
        }
    }
    return false;
}

void CompositingInputsUpdater::updateRecursive(Layer* layer, UpdateType updateType, AncestorInfo info)
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
        Layer::AncestorDependentCompositingInputs properties;

        if (!layer->isRootLayer()) {
            properties.clippedAbsoluteBoundingBox = enclosingIntRect(m_geometryMap.absoluteRect(layer->boundingBoxForCompositingOverlapTest()));
            // FIXME: Setting the absBounds to 1x1 instead of 0x0 makes very little sense,
            // but removing this code will make JSGameBench sad.
            // See https://codereview.chromium.org/13912020/
            if (properties.clippedAbsoluteBoundingBox.isEmpty())
                properties.clippedAbsoluteBoundingBox.setSize(IntSize(1, 1));

            IntRect clipRect = pixelSnappedIntRect(layer->clipper().backgroundClipRect(ClipRectsContext(m_rootLayer, AbsoluteClipRects)).rect());
            properties.clippedAbsoluteBoundingBox.intersect(clipRect);

            const Layer* parent = layer->parent();
            properties.opacityAncestor = parent->isTransparent() ? parent : parent->opacityAncestor();
            properties.transformAncestor = parent->hasTransformRelatedProperty() ? parent : parent->transformAncestor();
            properties.filterAncestor = parent->hasFilter() ? parent : parent->filterAncestor();

            if (info.hasAncestorWithClipOrOverflowClip) {
                const Layer* parentLayerOnClippingContainerChain = findParentLayerOnClippingContainerChain(layer);
                const bool parentHasClipOrOverflowClip = parentLayerOnClippingContainerChain->renderer()->hasClipOrOverflowClip();
                properties.clippingContainer = parentHasClipOrOverflowClip ? parentLayerOnClippingContainerChain->renderer() : parentLayerOnClippingContainerChain->clippingContainer();
            }

            if (info.lastScrollingAncestor) {
                const LayoutObject* containingBlock = layer->renderer()->containingBlock();
                const Layer* parentLayerOnContainingBlockChain = findParentLayerOnContainingBlockChain(containingBlock);

                properties.ancestorScrollingLayer = parentLayerOnContainingBlockChain->ancestorScrollingLayer();
                if (parentLayerOnContainingBlockChain->scrollsOverflow())
                    properties.ancestorScrollingLayer = parentLayerOnContainingBlockChain;

                if (layer->renderer()->isOutOfFlowPositioned() && !layer->subtreeIsInvisible()) {
                    const Layer* clippingLayer = properties.clippingContainer ? properties.clippingContainer->enclosingLayer() : layer->compositor()->rootLayer();
                    if (hasClippedStackingAncestor(layer, clippingLayer))
                        properties.clipParent = clippingLayer;
                }

                if (!layer->stackingNode()->isNormalFlowOnly()
                    && properties.ancestorScrollingLayer
                    && !info.ancestorStackingContext->renderer()->isDescendantOf(properties.ancestorScrollingLayer->renderer()))
                    properties.scrollParent = properties.ancestorScrollingLayer;
            }
        }

        properties.hasAncestorWithClipPath = info.hasAncestorWithClipPath;
        layer->updateAncestorDependentCompositingInputs(properties);
    }

    if (layer->stackingNode()->isStackingContext())
        info.ancestorStackingContext = layer;

    if (layer->scrollsOverflow())
        info.lastScrollingAncestor = layer;

    if (layer->renderer()->hasClipOrOverflowClip())
        info.hasAncestorWithClipOrOverflowClip = true;

    if (layer->renderer()->hasClipPath())
        info.hasAncestorWithClipPath = true;

    Layer::DescendantDependentCompositingInputs descendantProperties;
    for (Layer* child = layer->firstChild(); child; child = child->nextSibling()) {
        updateRecursive(child, updateType, info);

        descendantProperties.hasDescendantWithClipPath |= child->hasDescendantWithClipPath() || child->renderer()->hasClipPath();
        descendantProperties.hasNonIsolatedDescendantWithBlendMode |= (!child->stackingNode()->isStackingContext() && child->hasNonIsolatedDescendantWithBlendMode()) || child->renderer()->style()->hasBlendMode();
    }

    layer->updateDescendantDependentCompositingInputs(descendantProperties);
    layer->didUpdateCompositingInputs();

    m_geometryMap.popMappingsToAncestor(layer->parent());
}

#if ENABLE(ASSERT)

void CompositingInputsUpdater::assertNeedsCompositingInputsUpdateBitsCleared(Layer* layer)
{
    ASSERT(!layer->childNeedsCompositingInputsUpdate());
    ASSERT(!layer->needsCompositingInputsUpdate());

    for (Layer* child = layer->firstChild(); child; child = child->nextSibling())
        assertNeedsCompositingInputsUpdateBitsCleared(child);
}

#endif

} // namespace blink
