// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/compositing/CompositingReasonFinder.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
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
    updateTriggers();
}

void CompositingReasonFinder::updateTriggers()
{
    m_compositingTriggers = 0;

    Settings& settings = m_renderView.document().page()->settings();
    if (settings.acceleratedCompositingForVideoEnabled())
        m_compositingTriggers |= VideoTrigger;
    if (settings.acceleratedCompositingForPluginsEnabled())
        m_compositingTriggers |= PluginTrigger;
    if (settings.acceleratedCompositingForCanvasEnabled())
        m_compositingTriggers |= CanvasTrigger;
    if (settings.compositedScrollingForFramesEnabled())
        m_compositingTriggers |= ScrollableInnerFrameTrigger;
    if (settings.acceleratedCompositingForFiltersEnabled())
        m_compositingTriggers |= FilterTrigger;
    if (settings.acceleratedCompositingForGpuRasterizationHintEnabled())
        m_compositingTriggers |= GPURasterizationTrigger;

    // We map both these settings to universal overlow scrolling.
    // FIXME: Replace these settings with a generic compositing setting for HighDPI.
    if (settings.acceleratedCompositingForOverflowScrollEnabled() || settings.compositorDrivenAcceleratedScrollingEnabled())
        m_compositingTriggers |= OverflowScrollTrigger;

    // FIXME: acceleratedCompositingForFixedPositionEnabled should be renamed acceleratedCompositingForViewportConstrainedPositionEnabled().
    // Or the sticky and fixed position elements should be behind different flags.
    if (settings.acceleratedCompositingForFixedPositionEnabled())
        m_compositingTriggers |= ViewportConstrainedPositionedTrigger;
}

bool CompositingReasonFinder::hasOverflowScrollTrigger() const
{
    return m_compositingTriggers & OverflowScrollTrigger;
}

// FIXME: This is a temporary trigger for enabling the old, opt-in path for
// accelerated overflow scroll. It should be removed once the "universal"
// path is ready (crbug.com/254111).
bool CompositingReasonFinder::hasLegacyOverflowScrollTrigger() const
{
    return m_compositingTriggers & LegacyOverflowScrollTrigger;
}

bool CompositingReasonFinder::isMainFrame() const
{
    // FIXME: LocalFrame::isMainFrame() is probably better.
    return !m_renderView.document().ownerElement();
}

CompositingReasons CompositingReasonFinder::suppressWillChangeAndAnimationForGpuRasterization(const RenderLayer* layer, CompositingReasons styleReasons) const
{
    CompositingReasons adjustedReasons = styleReasons;
    adjustedReasons &= ~(CompositingReasonWillChangeCompositingHint | CompositingReasonWillChangeGpuRasterizationHint);

    // We can suppress layer creation for animations before animations start, but not
    // once they're already running on the compositor.
    if (!layer->renderer()->style()->isRunningAnimationOnCompositor())
        adjustedReasons &= ~CompositingReasonActiveAnimation;

    return adjustedReasons;
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

    if (requiresCompositingForAnimation(renderer))
        directReasons |= CompositingReasonActiveAnimation;

    if (requiresCompositingForFilters(renderer))
        directReasons |= CompositingReasonFilters;

    if (requiresCompositingForWillChangeCompositingHint(renderer))
        directReasons |= CompositingReasonWillChangeCompositingHint;

    if (requiresCompositingForWillChangeGpuRasterizationHint(renderer))
        directReasons |= CompositingReasonWillChangeGpuRasterizationHint;

    ASSERT(!(directReasons & ~CompositingReasonComboAllStyleDeterminedReasons));
    return directReasons;
}

bool CompositingReasonFinder::requiresCompositingForTransform(RenderObject* renderer) const
{
    // Note that we ask the renderer if it has a transform, because the style may have transforms,
    // but the renderer may be an inline that doesn't suppport them.
    return renderer->hasTransform() && renderer->style()->transform().has3DOperation();
}

bool CompositingReasonFinder::requiresCompositingForBackfaceVisibilityHidden(RenderObject* renderer) const
{
    return renderer->style()->backfaceVisibility() == BackfaceVisibilityHidden;
}

bool CompositingReasonFinder::requiresCompositingForFilters(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & FilterTrigger))
        return false;

    return renderer->hasFilter();
}

bool CompositingReasonFinder::requiresCompositingForWillChangeCompositingHint(const RenderObject* renderer) const
{
    return renderer->style()->hasWillChangeCompositingHint();
}

bool CompositingReasonFinder::requiresCompositingForWillChangeGpuRasterizationHint(const RenderObject* renderer) const
{
    if (!(m_compositingTriggers & GPURasterizationTrigger))
        return false;

    return renderer->style()->hasWillChangeGpuRasterizationHint();
}

CompositingReasons CompositingReasonFinder::nonStyleDeterminedDirectReasons(const RenderLayer* layer, bool* needToRecomputeCompositingRequirements) const
{
    CompositingReasons directReasons = CompositingReasonNone;
    RenderObject* renderer = layer->renderer();

    if (hasOverflowScrollTrigger()) {
        if (requiresCompositingForOutOfFlowClipping(layer))
            directReasons |= CompositingReasonOutOfFlowClipping;

        if (requiresCompositingForOverflowScrollingParent(layer))
            directReasons |= CompositingReasonOverflowScrollingParent;
    }

    if (requiresCompositingForOverflowScrolling(layer))
        directReasons |= CompositingReasonOverflowScrollingTouch;

    if (requiresCompositingForPositionSticky(renderer, layer))
        directReasons |= CompositingReasonPositionSticky;

    if (requiresCompositingForPositionFixed(renderer, layer, 0, needToRecomputeCompositingRequirements))
        directReasons |= CompositingReasonPositionFixed;

    directReasons |= renderer->additionalCompositingReasons(m_compositingTriggers);

    ASSERT(!(directReasons & CompositingReasonComboAllStyleDeterminedReasons));
    return directReasons;
}

bool CompositingReasonFinder::requiresCompositingForAnimation(RenderObject* renderer) const
{
    return renderer->style()->shouldCompositeForCurrentAnimations();
}

bool CompositingReasonFinder::requiresCompositingForOutOfFlowClipping(const RenderLayer* layer) const
{
    return layer->isUnclippedDescendant();
}

bool CompositingReasonFinder::requiresCompositingForOverflowScrollingParent(const RenderLayer* layer) const
{
    if (!hasOverflowScrollTrigger())
        return false;
    return layer->scrollParent();
}

bool CompositingReasonFinder::requiresCompositingForOverflowScrolling(const RenderLayer* layer) const
{
    return layer->needsCompositedScrolling();
}

static bool isViewportConstrainedStickyLayer(const RenderLayer* layer)
{
    ASSERT(layer->renderer()->isStickyPositioned());
    return !layer->enclosingOverflowClipLayer(ExcludeSelf);
}

bool CompositingReasonFinder::isViewportConstrainedFixedOrStickyLayer(const RenderLayer* layer)
{
    if (layer->renderer()->isStickyPositioned())
        return isViewportConstrainedStickyLayer(layer);

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
    return requiresCompositingForPositionSticky(renderer, layer) || requiresCompositingForPositionFixed(renderer, layer, viewportConstrainedNotCompositedReason, needToRecomputeCompositingRequirements);
}

bool CompositingReasonFinder::requiresCompositingForPositionSticky(RenderObject* renderer, const RenderLayer* layer) const
{
    if (!(m_compositingTriggers & ViewportConstrainedPositionedTrigger))
        return false;
    if (renderer->style()->position() != StickyPosition)
        return false;
    // FIXME: This probably isn't correct for accelerated overflow scrolling. crbug.com/361723
    // Instead it should return false only if the layer is not inside a scrollable region.
    return isViewportConstrainedStickyLayer(layer);
}

bool CompositingReasonFinder::requiresCompositingForPositionFixed(RenderObject* renderer, const RenderLayer* layer, RenderLayer::ViewportConstrainedNotCompositedReason* viewportConstrainedNotCompositedReason, bool* needToRecomputeCompositingRequirements) const
{
    if (!(m_compositingTriggers & ViewportConstrainedPositionedTrigger))
        return false;

    if (renderer->style()->position() != FixedPosition)
        return false;

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
        if (ancestor->scrollsOverflow())
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
        LayoutRect layerBounds = layer->boundingBoxForCompositing(layer->compositor()->rootRenderLayer(), RenderLayer::ApplyBoundsChickenEggHacks);
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
