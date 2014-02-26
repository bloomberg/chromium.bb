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
    , m_compositingTriggers(static_cast<ChromeClient::CompositingTriggerFlags>(ChromeClient::AllTriggers))
{
}

void CompositingReasonFinder::updateTriggers()
{
    m_compositingTriggers = m_renderView.document().page()->chrome().client().allowedCompositingTriggers();
}

bool CompositingReasonFinder::has3DTransformTrigger() const
{
    return m_compositingTriggers & ChromeClient::ThreeDTransformTrigger;
}

bool CompositingReasonFinder::hasAnimationTrigger() const
{
    return m_compositingTriggers & ChromeClient::AnimationTrigger;
}

bool CompositingReasonFinder::isMainFrame() const
{
    // FIXME: LocalFrame::isMainFrame() is probably better.
    return !m_renderView.document().ownerElement();
}

CompositingReasons CompositingReasonFinder::directReasons(const RenderLayer* layer, bool* needToRecomputeCompositingRequirements) const
{
    RenderObject* renderer = layer->renderer();
    CompositingReasons directReasons = CompositingReasonNone;

    if (requiresCompositingForTransform(renderer))
        directReasons |= CompositingReason3DTransform;

    // Only zero or one of the following conditions will be true for a given RenderLayer.
    if (requiresCompositingForVideo(renderer))
        directReasons |= CompositingReasonVideo;
    else if (requiresCompositingForCanvas(renderer))
        directReasons |= CompositingReasonCanvas;
    else if (requiresCompositingForPlugin(renderer, needToRecomputeCompositingRequirements))
        directReasons |= CompositingReasonPlugin;
    else if (requiresCompositingForFrame(renderer, needToRecomputeCompositingRequirements))
        directReasons |= CompositingReasonIFrame;

    if (requiresCompositingForBackfaceVisibilityHidden(renderer))
        directReasons |= CompositingReasonBackfaceVisibilityHidden;

    if (requiresCompositingForAnimation(renderer))
        directReasons |= CompositingReasonActiveAnimation;

    if (requiresCompositingForTransition(renderer))
        directReasons |= CompositingReasonTransitionProperty;

    if (requiresCompositingForFilters(renderer))
        directReasons |= CompositingReasonFilters;

    if (requiresCompositingForPosition(renderer, layer, 0, needToRecomputeCompositingRequirements))
        directReasons |= renderer->style()->position() == FixedPosition ? CompositingReasonPositionFixed : CompositingReasonPositionSticky;

    if (requiresCompositingForOverflowScrolling(layer))
        directReasons |= CompositingReasonOverflowScrollingTouch;

    if (requiresCompositingForOverflowScrollingParent(layer))
        directReasons |= CompositingReasonOverflowScrollingParent;

    if (requiresCompositingForOutOfFlowClipping(layer))
        directReasons |= CompositingReasonOutOfFlowClipping;

    return directReasons;
}

bool CompositingReasonFinder::requiresCompositingForScrollableFrame() const
{
    // Need this done first to determine overflow.
    ASSERT(!m_renderView.needsLayout());
    if (isMainFrame())
        return false;

    if (!(m_compositingTriggers & ChromeClient::ScrollableInnerFrameTrigger))
        return false;

    FrameView* frameView = m_renderView.frameView();
    return frameView->isScrollable();
}

bool CompositingReasonFinder::requiresCompositingForTransform(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::ThreeDTransformTrigger))
        return false;

    RenderStyle* style = renderer->style();
    // Note that we ask the renderer if it has a transform, because the style may have transforms,
    // but the renderer may be an inline that doesn't suppport them.
    return renderer->hasTransform() && style->transform().has3DOperation();
}

bool CompositingReasonFinder::requiresCompositingForVideo(RenderObject* renderer) const
{
    if (RuntimeEnabledFeatures::overlayFullscreenVideoEnabled() && renderer->isVideo()) {
        HTMLMediaElement* media = toHTMLMediaElement(renderer->node());
        if (media->isFullscreen())
            return true;
    }

    if (!(m_compositingTriggers & ChromeClient::VideoTrigger))
        return false;

    if (renderer->isVideo()) {
        RenderVideo* video = toRenderVideo(renderer);
        return video->shouldDisplayVideo() && video->supportsAcceleratedRendering();
    }
    return false;
}

bool CompositingReasonFinder::requiresCompositingForCanvas(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::CanvasTrigger))
        return false;

    if (renderer->isCanvas()) {
        HTMLCanvasElement* canvas = toHTMLCanvasElement(renderer->node());
        return canvas->renderingContext() && canvas->renderingContext()->isAccelerated();
    }
    return false;
}

bool CompositingReasonFinder::requiresCompositingForPlugin(RenderObject* renderer, bool* needToRecomputeCompositingRequirements) const
{
    if (!(m_compositingTriggers & ChromeClient::PluginTrigger))
        return false;

    if (!renderer->isEmbeddedObject() || !toRenderEmbeddedObject(renderer)->allowsAcceleratedCompositing())
        return false;

    // FIXME: this seems bogus. If we don't know the layout position/size of the plugin yet, would't that be handled elsewhere?
    *needToRecomputeCompositingRequirements = true;

    RenderWidget* pluginRenderer = toRenderWidget(renderer);
    // If we can't reliably know the size of the plugin yet, don't change compositing state.
    if (pluginRenderer->needsLayout())
        return pluginRenderer->hasLayer() && pluginRenderer->layer()->hasCompositedLayerMapping();

    // Don't go into compositing mode if height or width are zero, or size is 1x1.
    IntRect contentBox = pixelSnappedIntRect(pluginRenderer->contentBoxRect());
    return contentBox.height() * contentBox.width() > 1;
}

bool CompositingReasonFinder::requiresCompositingForFrame(RenderObject* renderer, bool* needToRecomputeCompositingRequirements) const
{
    if (!renderer->isRenderPart())
        return false;

    RenderPart* frameRenderer = toRenderPart(renderer);

    if (!frameRenderer->requiresAcceleratedCompositing())
        return false;

    if (frameRenderer->node() && frameRenderer->node()->isFrameOwnerElement() && toHTMLFrameOwnerElement(frameRenderer->node())->contentFrame() && toHTMLFrameOwnerElement(frameRenderer->node())->contentFrame()->remotePlatformLayer())
        return true;

    // FIXME: this seems bogus. If we don't know the layout position/size of the frame yet, wouldn't that be handled elsehwere?
    *needToRecomputeCompositingRequirements = true;

    RenderLayerCompositor* innerCompositor = RenderLayerCompositor::frameContentsCompositor(frameRenderer);
    if (!innerCompositor)
        return false;

    // If we can't reliably know the size of the iframe yet, don't change compositing state.
    if (renderer->needsLayout())
        return frameRenderer->hasLayer() && frameRenderer->layer()->hasCompositedLayerMapping();

    // Don't go into compositing mode if height or width are zero.
    IntRect contentBox = pixelSnappedIntRect(frameRenderer->contentBoxRect());
    return contentBox.height() * contentBox.width() > 0;
}

bool CompositingReasonFinder::requiresCompositingForBackfaceVisibilityHidden(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::ThreeDTransformTrigger))
        return false;

    return renderer->style()->backfaceVisibility() == BackfaceVisibilityHidden;
}

bool CompositingReasonFinder::requiresCompositingForAnimation(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::AnimationTrigger))
        return false;

    return shouldCompositeForActiveAnimations(*renderer);
}

bool CompositingReasonFinder::requiresCompositingForTransition(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::AnimationTrigger))
        return false;

    if (Settings* settings = m_renderView.document().settings()) {
        if (!settings->acceleratedCompositingForTransitionEnabled())
            return false;
    }

    return renderer->style()->transitionForProperty(CSSPropertyOpacity)
        || renderer->style()->transitionForProperty(CSSPropertyWebkitFilter)
        || renderer->style()->transitionForProperty(CSSPropertyWebkitTransform);
}

bool CompositingReasonFinder::requiresCompositingForFilters(RenderObject* renderer) const
{
    if (!(m_compositingTriggers & ChromeClient::FilterTrigger))
        return false;

    return renderer->hasFilter();
}

bool CompositingReasonFinder::requiresCompositingForOverflowScrollingParent(const RenderLayer* layer) const
{
    return !!layer->scrollParent();
}

bool CompositingReasonFinder::requiresCompositingForOutOfFlowClipping(const RenderLayer* layer) const
{
    return m_renderView.compositorDrivenAcceleratedScrollingEnabled() && layer->isUnclippedDescendant();
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
            | RenderLayer::ExcludeHiddenDescendants
            | RenderLayer::DontConstrainForMask
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

bool CompositingReasonFinder::requiresCompositingForOverflowScrolling(const RenderLayer* layer) const
{
    return layer->needsCompositedScrolling();
}

}
