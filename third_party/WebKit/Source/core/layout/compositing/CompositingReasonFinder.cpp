// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/compositing/CompositingReasonFinder.h"

#include "core/CSSPropertyNames.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/LayerCompositor.h"
#include "core/page/Page.h"

namespace blink {

CompositingReasonFinder::CompositingReasonFinder(LayoutView& layoutView)
    : m_layoutView(layoutView)
    , m_compositingTriggers(static_cast<CompositingTriggerFlags>(AllCompositingTriggers))
{
    updateTriggers();
}

void CompositingReasonFinder::updateTriggers()
{
    m_compositingTriggers = 0;

    Settings& settings = m_layoutView.document().page()->settings();
    if (settings.preferCompositingToLCDTextEnabled()) {
        m_compositingTriggers |= ScrollableInnerFrameTrigger;
        m_compositingTriggers |= OverflowScrollTrigger;
        m_compositingTriggers |= ViewportConstrainedPositionedTrigger;
    }
}

bool CompositingReasonFinder::hasOverflowScrollTrigger() const
{
    return m_compositingTriggers & OverflowScrollTrigger;
}

bool CompositingReasonFinder::isMainFrame() const
{
    // FIXME: LocalFrame::isMainFrame() is probably better.
    return !m_layoutView.document().ownerElement();
}

CompositingReasons CompositingReasonFinder::directReasons(const Layer* layer) const
{
    if (RuntimeEnabledFeatures::slimmingPaintCompositorLayerizationEnabled())
        return CompositingReasonNone;

    ASSERT(potentialCompositingReasonsFromStyle(layer->layoutObject()) == layer->potentialCompositingReasonsFromStyle());
    CompositingReasons styleDeterminedDirectCompositingReasons = layer->potentialCompositingReasonsFromStyle() & CompositingReasonComboAllDirectStyleDeterminedReasons;

    // Apply optimizations for scroll-blocks-on which require comparing style between objects.
    if ((styleDeterminedDirectCompositingReasons & CompositingReasonScrollBlocksOn) && !requiresCompositingForScrollBlocksOn(layer->layoutObject()))
        styleDeterminedDirectCompositingReasons &= ~CompositingReasonScrollBlocksOn;

    return styleDeterminedDirectCompositingReasons | nonStyleDeterminedDirectReasons(layer);
}

// This information doesn't appear to be incorporated into CompositingReasons.
bool CompositingReasonFinder::requiresCompositingForScrollableFrame() const
{
    // Need this done first to determine overflow.
    ASSERT(!m_layoutView.needsLayout());
    if (isMainFrame())
        return false;

    if (!(m_compositingTriggers & ScrollableInnerFrameTrigger))
        return false;

    return m_layoutView.frameView()->isScrollable();
}

CompositingReasons CompositingReasonFinder::potentialCompositingReasonsFromStyle(LayoutObject* renderer) const
{
    if (RuntimeEnabledFeatures::slimmingPaintCompositorLayerizationEnabled())
        return CompositingReasonNone;

    CompositingReasons reasons = CompositingReasonNone;

    const LayoutStyle& style = renderer->styleRef();

    if (requiresCompositingForTransform(renderer))
        reasons |= CompositingReason3DTransform;

    if (style.backfaceVisibility() == BackfaceVisibilityHidden)
        reasons |= CompositingReasonBackfaceVisibilityHidden;

    if (requiresCompositingForAnimation(style))
        reasons |= CompositingReasonActiveAnimation;

    if (style.hasWillChangeCompositingHint() && !style.subtreeWillChangeContents())
        reasons |= CompositingReasonWillChangeCompositingHint;

    if (style.hasInlineTransform())
        reasons |= CompositingReasonInlineTransform;

    if (style.transformStyle3D() == TransformStyle3DPreserve3D)
        reasons |= CompositingReasonPreserve3DWith3DDescendants;

    if (style.hasPerspective())
        reasons |= CompositingReasonPerspectiveWith3DDescendants;

    // Ignore scroll-blocks-on on the document element, because it will get propagated to
    // the LayoutView (by Document::inheritHtmlAndBodyElementStyles) and we don't want to
    // create two composited layers.
    if (style.hasScrollBlocksOn() && !renderer->isDocumentElement())
        reasons |= CompositingReasonScrollBlocksOn;

    // If the implementation of createsGroup changes, we need to be aware of that in this part of code.
    ASSERT((renderer->isTransparent() || renderer->hasMask() || renderer->hasFilter() || style.hasBlendMode()) == renderer->createsGroup());

    if (style.hasMask())
        reasons |= CompositingReasonMaskWithCompositedDescendants;

    if (style.hasFilter())
        reasons |= CompositingReasonFilterWithCompositedDescendants;

    // See Layer::updateTransform for an explanation of why we check both.
    if (renderer->hasTransformRelatedProperty() && style.hasTransform())
        reasons |= CompositingReasonTransformWithCompositedDescendants;

    if (renderer->isTransparent())
        reasons |= CompositingReasonOpacityWithCompositedDescendants;

    if (style.hasBlendMode())
        reasons |= CompositingReasonBlendingWithCompositedDescendants;

    if (renderer->hasReflection())
        reasons |= CompositingReasonReflectionWithCompositedDescendants;

    ASSERT(!(reasons & ~CompositingReasonComboAllStyleDeterminedReasons));
    return reasons;
}

bool CompositingReasonFinder::requiresCompositingForTransform(LayoutObject* renderer) const
{
    // Note that we ask the renderer if it has a transform, because the style may have transforms,
    // but the renderer may be an inline that doesn't support them.
    return renderer->hasTransformRelatedProperty() && renderer->style()->transform().has3DOperation();
}

CompositingReasons CompositingReasonFinder::nonStyleDeterminedDirectReasons(const Layer* layer) const
{
    CompositingReasons directReasons = CompositingReasonNone;
    LayoutObject* renderer = layer->layoutObject();

    if (hasOverflowScrollTrigger()) {
        if (layer->clipParent())
            directReasons |= CompositingReasonOutOfFlowClipping;

        if (layer->needsCompositedScrolling())
            directReasons |= CompositingReasonOverflowScrollingTouch;
    }

    // Composite |layer| if it is inside of an ancestor scrolling layer, but that
    // scrolling layer is not not on the stacking context ancestor chain of |layer|.
    // See the definition of the scrollParent property in Layer for more detail.
    if (const Layer* scrollingAncestor = layer->ancestorScrollingLayer()) {
        if (scrollingAncestor->needsCompositedScrolling() && layer->scrollParent())
            directReasons |= CompositingReasonOverflowScrollingParent;
    }

    if (requiresCompositingForPositionFixed(layer))
        directReasons |= CompositingReasonPositionFixed;

    directReasons |= renderer->additionalCompositingReasons();

    ASSERT(!(directReasons & CompositingReasonComboAllStyleDeterminedReasons));
    return directReasons;
}

bool CompositingReasonFinder::requiresCompositingForAnimation(const LayoutStyle& style) const
{
    if (style.subtreeWillChangeContents())
        return style.isRunningAnimationOnCompositor();

    return style.shouldCompositeForCurrentAnimations();
}

bool CompositingReasonFinder::requiresCompositingForPositionFixed(const Layer* layer) const
{
    if (!(m_compositingTriggers & ViewportConstrainedPositionedTrigger))
        return false;
    // Don't promote fixed position elements that are descendants of a non-view container, e.g. transformed elements.
    // They will stay fixed wrt the container rather than the enclosing frame.
    return layer->scrollsWithViewport() && m_layoutView.frameView()->isScrollable();
}

bool CompositingReasonFinder::requiresCompositingForScrollBlocksOn(const LayoutObject* renderer) const
{
    // Note that the other requires* functions run at LayoutObject::styleDidChange time and so can rely
    // only on the style of their object.  This function runs at CompositingRequirementsUpdater::update
    // time, and so can consider the style of other objects.
    const LayoutStyle& style = renderer->styleRef();

    // We should only get here by CompositingReasonScrollBlocksOn being a potential compositing reason.
    ASSERT(style.hasScrollBlocksOn() && !renderer->isDocumentElement());

    // scroll-blocks-on style is propagated from the document element to the document.
    ASSERT(!renderer->isLayoutView()
        || !renderer->document().documentElement()
        || !renderer->document().documentElement()->layoutObject()
        || renderer->document().documentElement()->layoutObject()->style()->scrollBlocksOn() == style.scrollBlocksOn());

    // When a scroll occurs, it's the union of all bits set on the target element's containing block
    // chain that determines the behavior.  Thus we really only need a new layer if this object contains
    // additional bits from those set by all objects in it's containing block chain.  But determining
    // this fully is probably more expensive than it's worth.  Instead we just have fast-paths here for
    // the most common cases of unnecessary layer creation.
    // Optimizing this fully would avoid layer explosion in pathological cases like '*' rules.
    // We could consider tracking the current state in CompositingRequirementsUpdater::update.

    // Ensure iframes don't get composited when they require no more blocking than their parent.
    if (renderer->isLayoutView()) {
        if (const FrameView* parentFrame = toLayoutView(renderer)->frameView()->parentFrameView()) {
            if (const LayoutView* parentRenderer = parentFrame->layoutView()) {
                // Does this frame contain only blocks-on bits already present in the parent frame?
                if (!(style.scrollBlocksOn() & ~parentRenderer->style()->scrollBlocksOn()))
                    return false;
            }
        } else {
            // The root frame will either always already be composited, or compositing will be disabled.
            // Either way, we don't need to require compositing for scroll blocks on.  This avoids
            // enabling compositing by default, and avoids cluttering the root layers compositing reasons.
            return false;
        }
    }

    return true;
}

}
