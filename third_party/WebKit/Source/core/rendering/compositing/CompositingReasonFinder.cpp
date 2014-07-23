// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/compositing/CompositingReasonFinder.h"

#include "core/CSSPropertyNames.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"

namespace blink {

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
    if (settings.acceleratedCompositingForCanvasEnabled())
        m_compositingTriggers |= CanvasTrigger;
    if (settings.compositedScrollingForFramesEnabled())
        m_compositingTriggers |= ScrollableInnerFrameTrigger;
    if (settings.acceleratedCompositingForFiltersEnabled())
        m_compositingTriggers |= FilterTrigger;

    // We map both these settings to universal overlow scrolling.
    // FIXME: Replace these settings with a generic compositing setting for HighDPI.
    if (settings.acceleratedCompositingForOverflowScrollEnabled() || settings.compositorDrivenAcceleratedScrollingEnabled())
        m_compositingTriggers |= OverflowScrollTrigger;

    if (settings.acceleratedCompositingForFixedPositionEnabled())
        m_compositingTriggers |= ViewportConstrainedPositionedTrigger;
}

bool CompositingReasonFinder::hasOverflowScrollTrigger() const
{
    return m_compositingTriggers & OverflowScrollTrigger;
}

bool CompositingReasonFinder::isMainFrame() const
{
    // FIXME: LocalFrame::isMainFrame() is probably better.
    return !m_renderView.document().ownerElement();
}

CompositingReasons CompositingReasonFinder::directReasons(const RenderLayer* layer) const
{
    ASSERT(potentialCompositingReasonsFromStyle(layer->renderer()) == layer->potentialCompositingReasonsFromStyle());
    CompositingReasons styleDeterminedDirectCompositingReasons = layer->potentialCompositingReasonsFromStyle() & CompositingReasonComboAllDirectStyleDeterminedReasons;
    return styleDeterminedDirectCompositingReasons | nonStyleDeterminedDirectReasons(layer);
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

    return m_renderView.frameView()->isScrollable();
}

CompositingReasons CompositingReasonFinder::potentialCompositingReasonsFromStyle(RenderObject* renderer) const
{
    CompositingReasons reasons = CompositingReasonNone;

    RenderStyle* style = renderer->style();

    if (requiresCompositingForTransform(renderer))
        reasons |= CompositingReason3DTransform;

    if (requiresCompositingForFilters(renderer))
        reasons |= CompositingReasonFilters;

    if (style->backfaceVisibility() == BackfaceVisibilityHidden)
        reasons |= CompositingReasonBackfaceVisibilityHidden;

    if (requiresCompositingForAnimation(style))
        reasons |= CompositingReasonActiveAnimation;

    if (style->hasWillChangeCompositingHint() && !style->subtreeWillChangeContents())
        reasons |= CompositingReasonWillChangeCompositingHint;

    if (style->hasInlineTransform())
        reasons |= CompositingReasonInlineTransform;

    if (style->transformStyle3D() == TransformStyle3DPreserve3D)
        reasons |= CompositingReasonPreserve3DWith3DDescendants;

    if (style->hasPerspective())
        reasons |= CompositingReasonPerspectiveWith3DDescendants;

    // If the implementation of createsGroup changes, we need to be aware of that in this part of code.
    ASSERT((renderer->isTransparent() || renderer->hasMask() || renderer->hasFilter() || renderer->hasBlendMode()) == renderer->createsGroup());

    if (style->hasMask())
        reasons |= CompositingReasonMaskWithCompositedDescendants;

    if (style->hasFilter())
        reasons |= CompositingReasonFilterWithCompositedDescendants;

    // See RenderLayer::updateTransform for an explanation of why we check both.
    if (renderer->hasTransform() && style->hasTransform())
        reasons |= CompositingReasonTransformWithCompositedDescendants;

    if (renderer->isTransparent())
        reasons |= CompositingReasonOpacityWithCompositedDescendants;

    if (renderer->hasBlendMode())
        reasons |= CompositingReasonBlendingWithCompositedDescendants;

    if (renderer->hasReflection())
        reasons |= CompositingReasonReflectionWithCompositedDescendants;

    ASSERT(!(reasons & ~CompositingReasonComboAllStyleDeterminedReasons));
    return reasons;
}

bool CompositingReasonFinder::requiresCompositingForTransform(RenderObject* renderer) const
{
    // Note that we ask the renderer if it has a transform, because the style may have transforms,
    // but the renderer may be an inline that doesn't suppport them.
    return renderer->hasTransform() && renderer->style()->transform().has3DOperation();
}

bool CompositingReasonFinder::requiresCompositingForFilters(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & FilterTrigger))
        return false;

    return renderer->hasFilter();
}

CompositingReasons CompositingReasonFinder::nonStyleDeterminedDirectReasons(const RenderLayer* layer) const
{
    CompositingReasons directReasons = CompositingReasonNone;
    RenderObject* renderer = layer->renderer();

    if (hasOverflowScrollTrigger()) {
        if (layer->clipParent())
            directReasons |= CompositingReasonOutOfFlowClipping;

        if (const RenderLayer* scrollingAncestor = layer->ancestorScrollingLayer()) {
            if (scrollingAncestor->needsCompositedScrolling() && layer->scrollParent())
                directReasons |= CompositingReasonOverflowScrollingParent;
        }

        if (layer->needsCompositedScrolling())
            directReasons |= CompositingReasonOverflowScrollingTouch;
    }

    if (requiresCompositingForPositionFixed(renderer, layer, 0))
        directReasons |= CompositingReasonPositionFixed;

    directReasons |= renderer->additionalCompositingReasons(m_compositingTriggers);

    ASSERT(!(directReasons & CompositingReasonComboAllStyleDeterminedReasons));
    return directReasons;
}

bool CompositingReasonFinder::requiresCompositingForAnimation(RenderStyle* style) const
{
    if (style->subtreeWillChangeContents())
        return style->isRunningAnimationOnCompositor();

    return style->shouldCompositeForCurrentAnimations();
}

bool CompositingReasonFinder::requiresCompositingForPositionFixed(RenderObject* renderer, const RenderLayer* layer, RenderLayer::ViewportConstrainedNotCompositedReason* viewportConstrainedNotCompositedReason) const
{
    if (!(m_compositingTriggers & ViewportConstrainedPositionedTrigger))
        return false;

    if (renderer->style()->position() != FixedPosition)
        return false;

    RenderObject* container = renderer->container();
    // If the renderer is not hooked up yet then we have to wait until it is.
    if (!container) {
        ASSERT(m_renderView.document().lifecycle().state() < DocumentLifecycle::InCompositingUpdate);
        // FIXME: Remove this and ASSERT(container) once we get rid of the incremental
        // allocateOrClearCompositedLayerMapping compositing update. This happens when
        // adding the renderer to the tree because we setStyle before addChild in
        // createRendererForElementIfNeeded.
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
    // FIXME: Get rid of this codepath once we get rid of the incremental compositing update in RenderLayer::styleChanged.
    if (m_renderView.document().lifecycle().state() < DocumentLifecycle::LayoutClean)
        return layer->hasCompositedLayerMapping();

    bool paintsContent = layer->isVisuallyNonEmpty() || layer->hasVisibleDescendant();
    if (!paintsContent) {
        if (viewportConstrainedNotCompositedReason)
            *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForNoVisibleContent;
        return false;
    }

    // Fixed position elements that are invisible in the current view don't get their own layer.
    if (FrameView* frameView = m_renderView.frameView()) {
        ASSERT(m_renderView.document().lifecycle().state() == DocumentLifecycle::InCompositingUpdate);
        LayoutRect viewBounds = frameView->viewportConstrainedVisibleContentRect();
        LayoutRect layerBounds = layer->boundingBoxForCompositing(layer->compositor()->rootRenderLayer(), RenderLayer::ApplyBoundsChickenEggHacks);
        if (!viewBounds.intersects(enclosingIntRect(layerBounds))) {
            if (viewportConstrainedNotCompositedReason)
                *viewportConstrainedNotCompositedReason = RenderLayer::NotCompositedForBoundsOutOfView;
            return false;
        }
    }

    return true;
}

}
