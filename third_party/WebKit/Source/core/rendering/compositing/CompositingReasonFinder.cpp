// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/compositing/CompositingReasonFinder.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "core/animation/ActiveAnimations.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/canvas/CanvasRenderingContext.h"
#include "core/page/Chrome.h"
#include "core/page/Page.h"
#include "core/rendering/RenderApplet.h"
#include "core/rendering/RenderEmbeddedObject.h"
#include "core/rendering/RenderFullScreen.h"
#include "core/rendering/RenderGeometryMap.h"
#include "core/rendering/RenderIFrame.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderLayerStackingNode.h"
#include "core/rendering/RenderLayerStackingNodeIterator.h"
#include "core/rendering/RenderReplica.h"
#include "core/rendering/RenderVideo.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"

namespace WebCore {

CompositingReasonFinder::CompositingReasonFinder(RenderView& renderView)
    : m_renderView(renderView)
    , m_compositingTriggers(static_cast<CompositingTriggerFlags>(AllCompositingTriggers))
{
}

void CompositingReasonFinder::updateTriggers()
{
    m_compositingTriggers = m_renderView.document().page()->chrome().client().allowedCompositingTriggers();
}

bool CompositingReasonFinder::has3DTransformTrigger() const
{
    return m_compositingTriggers & ThreeDTransformTrigger;
}

bool CompositingReasonFinder::hasAnimationTrigger() const
{
    return m_compositingTriggers & AnimationTrigger;
}

bool CompositingReasonFinder::isMainFrame() const
{
    // FIXME: LocalFrame::isMainFrame() is probably better.
    return !m_renderView.document().ownerElement();
}

CompositingReasons CompositingReasonFinder::directReasons(const RenderLayer* layer, bool* needToRecomputeCompositingRequirements) const
{
    CompositingReasons styleReasons = layer->styleDeterminedCompositingReasons();
    ASSERT(styleDeterminedReasons(layer->renderer()) == styleReasons);
    return styleReasons | nonStyleDeterminedDirectReasons(layer, needToRecomputeCompositingRequirements);
}

// This information doesn't appear to be incorporated into CompositingReasons.
bool CompositingReasonFinder::requiresCompositingForScrollableFrame() const
{
    // Need this done first to determine overflow.
    ASSERT(!m_renderView.needsLayout());
    if (isMainFrame())
        return false;

    if (!(m_compositingTriggers & ScrollableInnerFrameTrigger))
        return false;

    FrameView* frameView = m_renderView.frameView();
    return frameView->isScrollable();
}

CompositingReasons CompositingReasonFinder::styleDeterminedReasons(RenderObject* renderer) const
{
    CompositingReasons directReasons = CompositingReasonNone;

    if (requiresCompositingForTransform(renderer))
        directReasons |= CompositingReason3DTransform;

    if (requiresCompositingForBackfaceVisibilityHidden(renderer))
        directReasons |= CompositingReasonBackfaceVisibilityHidden;

    if (requiresCompositingForFilters(renderer))
        directReasons |= CompositingReasonFilters;

    if (requiresCompositingForWillChange(renderer))
        directReasons |= CompositingReasonWillChange;

    ASSERT(!(directReasons & ~CompositingReasonComboAllStyleDeterminedReasons));
    return directReasons;
}

bool CompositingReasonFinder::requiresCompositingForTransform(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ThreeDTransformTrigger))
        return false;

    // Note that we ask the renderer if it has a transform, because the style may have transforms,
    // but the renderer may be an inline that doesn't suppport them.
    return renderer->hasTransform() && renderer->style()->transform().has3DOperation();
}

bool CompositingReasonFinder::requiresCompositingForBackfaceVisibilityHidden(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ThreeDTransformTrigger))
        return false;

    return renderer->style()->backfaceVisibility() == BackfaceVisibilityHidden;
}

bool CompositingReasonFinder::requiresCompositingForFilters(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & FilterTrigger))
        return false;

    return renderer->hasFilter();
}

bool CompositingReasonFinder::requiresCompositingForWillChange(const RenderObject* renderer) const
{
    if (renderer->style()->hasWillChangeCompositingHint())
        return true;

    if (!(m_compositingTriggers & GPURasterizationTrigger))
        return false;

    return renderer->style()->hasWillChangeGpuRasterizationHint();
}

CompositingReasons CompositingReasonFinder::nonStyleDeterminedDirectReasons(const RenderLayer* layer, bool* needToRecomputeCompositingRequirements) const
{
    CompositingReasons directReasons = CompositingReasonNone;
    RenderObject* renderer = layer->renderer();

    if (requiresCompositingForAnimation(renderer))
        directReasons |= CompositingReasonActiveAnimation;

    if (m_renderView.compositorDrivenAcceleratedScrollingEnabled()) {
        if (requiresCompositingForOutOfFlowClipping(layer))
            directReasons |= CompositingReasonOutOfFlowClipping;

        if (requiresCompositingForOverflowScrollingParent(layer))
            directReasons |= CompositingReasonOverflowScrollingParent;
    }

    if (requiresCompositingForOverflowScrolling(layer))
        directReasons |= CompositingReasonOverflowScrollingTouch;

    if (requiresCompositingForPosition(renderer, layer, 0, needToRecomputeCompositingRequirements))
        directReasons |= renderer->style()->position() == FixedPosition ? CompositingReasonPositionFixed : CompositingReasonPositionSticky;

    directReasons |= renderer->additionalCompositingReasons(m_compositingTriggers);

    ASSERT(!(directReasons & CompositingReasonComboAllStyleDeterminedReasons));
    return directReasons;
}

bool CompositingReasonFinder::requiresCompositingForAnimation(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & AnimationTrigger))
        return false;

    return shouldCompositeForActiveAnimations(*renderer);
}

bool CompositingReasonFinder::requiresCompositingForOutOfFlowClipping(const RenderLayer* layer) const
{
    return layer->isUnclippedDescendant();
}

bool CompositingReasonFinder::requiresCompositingForOverflowScrollingParent(const RenderLayer* layer) const
{
    return layer->scrollParent();
}

bool CompositingReasonFinder::requiresCompositingForOverflowScrolling(const RenderLayer* layer) const
{
    return layer->needsCompositedScrolling();
}

bool CompositingReasonFinder::isViewportConstrainedFixedOrStickyLayer(const RenderLayer* layer)
{
    if (layer->renderer()->isStickyPositioned())
        return !layer->enclosingOverflowClipLayer(ExcludeSelf);

    if (layer->renderer()->style()->position() != FixedPosition)
        return false;

    for (const RenderLayerStackingNode* stackingContainer = layer->stackingNode(); stackingContainer;
        stackingContainer = stackingContainer->ancestorStackingContainerNode()) {
        if (stackingContainer->layer()->compositingState() != NotComposited
            && stackingContainer->layer()->renderer()->style()->position() == FixedPosition)
            return false;
    }

    return true;
}

bool CompositingReasonFinder::requiresCompositingForPosition(RenderObject* renderer, const RenderLayer* layer, RenderLayer::ViewportConstrainedNotCompositedReason* viewportConstrainedNotCompositedReason, bool* needToRecomputeCompositingRequirements) const
{
    // position:fixed elements that create their own stacking context (e.g. have an explicit z-index,
    // opacity, transform) can get their own composited layer. A stacking context is required otherwise
    // z-index and clipping will be broken.
    if (!renderer->isPositioned())
        return false;

    EPosition position = renderer->style()->position();
    bool isFixed = renderer->isOutOfFlowPositioned() && position == FixedPosition;
    // FIXME: The isStackingContainer check here is redundant. Fixed position elements are always stacking contexts.
    if (isFixed && !layer->stackingNode()->isStackingContainer())
        return false;

    bool isSticky = renderer->isInFlowPositioned() && position == StickyPosition;
    if (!isFixed && !isSticky)
        return false;

    // FIXME: acceleratedCompositingForFixedPositionEnabled should probably be renamed acceleratedCompositingForViewportConstrainedPositionEnabled().
    if (Settings* settings = m_renderView.document().settings()) {
        if (!settings->acceleratedCompositingForFixedPositionEnabled())
            return false;
    }

    if (isSticky)
        return isViewportConstrainedFixedOrStickyLayer(layer);

    RenderObject* container = renderer->container();
    // If the renderer is not hooked up yet then we have to wait until it is.
    if (!container) {
        *needToRecomputeCompositingRequirements = true;
        return false;
    }

    // Don't promote fixed position elements that are descendants of a non-view container, e.g. transformed elements.
    // They will stay fixed wrt the container rather than the enclosing frame.
    if (container != &m_renderView) {
        if (viewportConstrainedNotCompositedReason)
            *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForNonViewContainer;
        return false;
    }

    // If the fixed-position element does not have any scrollable ancestor between it and
    // its container, then we do not need to spend compositor resources for it. Start by
    // assuming we can opt-out (i.e. no scrollable ancestor), and refine the answer below.
    bool hasScrollableAncestor = false;

    // The FrameView has the scrollbars associated with the top level viewport, so we have to
    // check the FrameView in addition to the hierarchy of ancestors.
    FrameView* frameView = m_renderView.frameView();
    if (frameView && frameView->isScrollable())
        hasScrollableAncestor = true;

    RenderLayer* ancestor = layer->parent();
    while (ancestor && !hasScrollableAncestor) {
        if (frameView->containsScrollableArea(ancestor->scrollableArea()))
            hasScrollableAncestor = true;
        if (ancestor->renderer() == &m_renderView)
            break;
        ancestor = ancestor->parent();
    }

    if (!hasScrollableAncestor) {
        if (viewportConstrainedNotCompositedReason)
            *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForUnscrollableAncestors;
        return false;
    }

    // Subsequent tests depend on layout. If we can't tell now, just keep things the way they are until layout is done.
    if (m_renderView.document().lifecycle().state() < DocumentLifecycle::LayoutClean) {
        *needToRecomputeCompositingRequirements = true;
        return layer->hasCompositedLayerMapping();
    }

    bool paintsContent = layer->isVisuallyNonEmpty() || layer->hasVisibleDescendant();
    if (!paintsContent) {
        if (viewportConstrainedNotCompositedReason)
            *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForNoVisibleContent;
        return false;
    }

    // Fixed position elements that are invisible in the current view don't get their own layer.
    if (FrameView* frameView = m_renderView.frameView()) {
        LayoutRect viewBounds = frameView->viewportConstrainedVisibleContentRect();
        LayoutRect layerBounds = layer->calculateLayerBounds(layer->compositor()->rootRenderLayer(), 0,
            RenderLayer::DefaultCalculateLayerBoundsFlags
            | RenderLayer::IncludeCompositedDescendants
            | RenderLayer::PretendLayerHasOwnBacking);
        if (!viewBounds.intersects(enclosingIntRect(layerBounds))) {
            if (viewportConstrainedNotCompositedReason) {
                *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForBoundsOutOfView;
                *needToRecomputeCompositingRequirements = true;
            }
            return false;
        }
    }

    return true;
}

}
