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
    if (settings.acceleratedCompositingForCanvasEnabled())
        m_compositingTriggers |= CanvasTrigger;
    if (settings.compositedScrollingForFramesEnabled())
        m_compositingTriggers |= ScrollableInnerFrameTrigger;

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

    if (requiresCompositingForPositionFixed(renderer))
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

bool CompositingReasonFinder::requiresCompositingForPositionFixed(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ViewportConstrainedPositionedTrigger))
        return false;
    // Don't promote fixed position elements that are descendants of a non-view container, e.g. transformed elements.
    // They will stay fixed wrt the container rather than the enclosing frame.
    return renderer->style()->position() == FixedPosition
        && renderer->container() == &m_renderView
        && m_renderView.frameView()->isScrollable();
}

}
