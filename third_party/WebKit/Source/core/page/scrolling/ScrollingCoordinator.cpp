/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/page/scrolling/ScrollingCoordinator.h"

#include "core/dom/Document.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/platform/PlatformWheelEvent.h"
#include "core/platform/ScrollAnimator.h"
#include "core/platform/ScrollbarThemeComposite.h"
#include "core/platform/chromium/support/WebScrollbarImpl.h"
#include "core/platform/chromium/support/WebScrollbarThemeGeometryNative.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/platform/graphics/IntRect.h"
#include "core/platform/graphics/Region.h"
#include "core/platform/graphics/chromium/GraphicsLayerChromium.h"
#include "core/plugins/PluginView.h"
#include "core/rendering/RenderLayerBacking.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebLayerPositionConstraint.h>
#include <public/WebScrollbar.h>
#include <public/WebScrollbarLayer.h>
#include <public/WebScrollbarThemeGeometry.h>
#include <public/WebScrollbarThemePainter.h>

using WebKit::WebLayer;
using WebKit::WebLayerPositionConstraint;
using WebKit::WebRect;
using WebKit::WebScrollbarLayer;
using WebKit::WebVector;


namespace WebCore {

static WebLayer* scrollingWebLayerForGraphicsLayer(GraphicsLayer* layer)
{
    return layer->platformLayer();
}

WebLayer* ScrollingCoordinator::scrollingWebLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    GraphicsLayer* graphicsLayer = scrollLayerForScrollableArea(scrollableArea);
    return graphicsLayer ? scrollingWebLayerForGraphicsLayer(graphicsLayer) : 0;
}

PassRefPtr<ScrollingCoordinator> ScrollingCoordinator::create(Page* page)
{
    return adoptRef(new ScrollingCoordinator(page));
}

ScrollingCoordinator::ScrollingCoordinator(Page* page)
    : m_page(page)
{
}

ScrollingCoordinator::~ScrollingCoordinator()
{
    ASSERT(!m_page);
    for (ScrollbarMap::iterator it = m_horizontalScrollbars.begin(); it != m_horizontalScrollbars.end(); ++it)
        GraphicsLayerChromium::unregisterContentsLayer(it->value->layer());
    for (ScrollbarMap::iterator it = m_verticalScrollbars.begin(); it != m_verticalScrollbars.end(); ++it)
        GraphicsLayerChromium::unregisterContentsLayer(it->value->layer());

}

void ScrollingCoordinator::setShouldHandleScrollGestureOnMainThreadRegion(const Region& region)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view())) {
        Vector<IntRect> rects = region.rects();
        WebVector<WebRect> webRects(rects.size());
        for (size_t i = 0; i < rects.size(); ++i)
            webRects[i] = rects[i];
        scrollLayer->setNonFastScrollableRegion(webRects);
    }
}

void ScrollingCoordinator::frameViewLayoutUpdated(FrameView* frameView)
{
    ASSERT(m_page);

    // Compute the region of the page that we can't handle scroll gestures on impl thread:
    // This currently includes:
    // 1. All scrollable areas, such as subframes, overflow divs and list boxes, whose composited
    // scrolling are not enabled. We need to do this even if the frame view whose layout was updated
    // is not the main frame.
    // 2. Resize control areas, e.g. the small rect at the right bottom of div/textarea/iframe when
    // CSS property "resize" is enabled.
    // 3. Plugin areas.
    Region shouldHandleScrollGestureOnMainThreadRegion = computeShouldHandleScrollGestureOnMainThreadRegion(m_page->mainFrame(), IntPoint());
    setShouldHandleScrollGestureOnMainThreadRegion(shouldHandleScrollGestureOnMainThreadRegion);
    Vector<IntRect> touchEventTargetRects;
    computeAbsoluteTouchEventTargetRects(m_page->mainFrame()->document(), touchEventTargetRects);
    setTouchEventTargetRects(touchEventTargetRects);
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(frameView))
        scrollLayer->setBounds(frameView->contentsSize());
}

void ScrollingCoordinator::setLayerIsContainerForFixedPositionLayers(GraphicsLayer* layer, bool enable)
{
    if (WebLayer* scrollableLayer = scrollingWebLayerForGraphicsLayer(layer))
        scrollableLayer->setIsContainerForFixedPositionLayers(enable);
}

static void clearPositionConstraintExceptForLayer(GraphicsLayer* layer, GraphicsLayer* except)
{
    if (layer && layer != except && scrollingWebLayerForGraphicsLayer(layer))
        scrollingWebLayerForGraphicsLayer(layer)->setPositionConstraint(WebLayerPositionConstraint());
}

static WebLayerPositionConstraint computePositionConstraint(const RenderLayer* layer)
{
    ASSERT(layer->isComposited());
    do {
        if (layer->renderer()->style()->position() == FixedPosition) {
            const RenderObject* fixedPositionObject = layer->renderer();
            bool fixedToRight = !fixedPositionObject->style()->right().isAuto();
            bool fixedToBottom = !fixedPositionObject->style()->bottom().isAuto();
            return WebLayerPositionConstraint::fixedPosition(fixedToRight, fixedToBottom);
        }

        layer = layer->parent();
    } while (layer && !layer->isComposited());
    return WebLayerPositionConstraint();
}

void ScrollingCoordinator::updateLayerPositionConstraint(RenderLayer* layer)
{
    ASSERT(layer->backing());
    RenderLayerBacking* backing = layer->backing();
    GraphicsLayer* mainLayer = backing->childForSuperlayers();

    // Avoid unnecessary commits
    clearPositionConstraintExceptForLayer(backing->ancestorClippingLayer(), mainLayer);
    clearPositionConstraintExceptForLayer(backing->contentsContainmentLayer(), mainLayer);
    clearPositionConstraintExceptForLayer(backing->graphicsLayer(), mainLayer);

    if (WebLayer* scrollableLayer = scrollingWebLayerForGraphicsLayer(mainLayer))
        scrollableLayer->setPositionConstraint(computePositionConstraint(layer));
}

void ScrollingCoordinator::willDestroyScrollableArea(ScrollableArea* scrollableArea)
{
    removeWebScrollbarLayer(scrollableArea, HorizontalScrollbar);
    removeWebScrollbarLayer(scrollableArea, VerticalScrollbar);
}

void ScrollingCoordinator::removeWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    if (OwnPtr<WebScrollbarLayer> scrollbarLayer = scrollbars.take(scrollableArea))
        GraphicsLayerChromium::unregisterContentsLayer(scrollbarLayer->layer());
}

static PassOwnPtr<WebScrollbarLayer> createScrollbarLayer(Scrollbar* scrollbar)
{
    // All Chromium scrollbar themes derive from ScrollbarThemeComposite.
    ScrollbarThemeComposite* themeComposite = static_cast<ScrollbarThemeComposite*>(scrollbar->theme());
    WebKit::WebScrollbarThemePainter painter(themeComposite, scrollbar);
    OwnPtr<WebKit::WebScrollbarThemeGeometry> geometry(WebKit::WebScrollbarThemeGeometryNative::create(themeComposite));

    OwnPtr<WebScrollbarLayer> scrollbarLayer = adoptPtr(WebKit::Platform::current()->compositorSupport()->createScrollbarLayer(new WebKit::WebScrollbarImpl(scrollbar), painter, geometry.leakPtr()));
    GraphicsLayerChromium::registerContentsLayer(scrollbarLayer->layer());
    return scrollbarLayer.release();
}

static void detachScrollbarLayer(GraphicsLayer* scrollbarGraphicsLayer)
{
    ASSERT(scrollbarGraphicsLayer);

    scrollbarGraphicsLayer->setContentsToPlatformLayer(0);
    scrollbarGraphicsLayer->setDrawsContent(true);
}

static void setupScrollbarLayer(GraphicsLayer* scrollbarGraphicsLayer, WebScrollbarLayer* scrollbarLayer, WebLayer* scrollLayer)
{
    ASSERT(scrollbarGraphicsLayer);
    ASSERT(scrollbarLayer);

    if (!scrollLayer) {
        detachScrollbarLayer(scrollbarGraphicsLayer);
        return;
    }
    scrollbarLayer->setScrollLayer(scrollLayer);
    scrollbarGraphicsLayer->setContentsToPlatformLayer(scrollbarLayer->layer());
    scrollbarGraphicsLayer->setDrawsContent(false);
}

WebScrollbarLayer* ScrollingCoordinator::addWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, PassOwnPtr<WebKit::WebScrollbarLayer> scrollbarLayer)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    return scrollbars.add(scrollableArea, scrollbarLayer).iterator->value.get();
}

WebScrollbarLayer* ScrollingCoordinator::getWebScrollbarLayer(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
    ScrollbarMap& scrollbars = orientation == HorizontalScrollbar ? m_horizontalScrollbars : m_verticalScrollbars;
    return scrollbars.get(scrollableArea);
}

void ScrollingCoordinator::scrollableAreaScrollbarLayerDidChange(ScrollableArea* scrollableArea, ScrollbarOrientation orientation)
{
// FIXME: Instead of hardcode here, we should make a setting flag.
#if OS(DARWIN)
    static const bool platformSupportsCoordinatedScrollbar = false;
    static const bool platformSupportsMainFrameOnly = false; // Don't care.
#elif OS(ANDROID)
    static const bool platformSupportsCoordinatedScrollbar = true;
    static const bool platformSupportsMainFrameOnly = false;
#else
    static const bool platformSupportsCoordinatedScrollbar = true;
    static const bool platformSupportsMainFrameOnly = true;
#endif
    if (!platformSupportsCoordinatedScrollbar)
        return;

    bool isMainFrame = (scrollableArea == static_cast<ScrollableArea*>(m_page->mainFrame()->view()));
    if (!isMainFrame && platformSupportsMainFrameOnly)
        return;

    GraphicsLayer* scrollbarGraphicsLayer = orientation == HorizontalScrollbar ? horizontalScrollbarLayerForScrollableArea(scrollableArea) : verticalScrollbarLayerForScrollableArea(scrollableArea);
    if (scrollbarGraphicsLayer) {
        Scrollbar* scrollbar = orientation == HorizontalScrollbar ? scrollableArea->horizontalScrollbar() : scrollableArea->verticalScrollbar();
        if (scrollbar->isCustomScrollbar()) {
            detachScrollbarLayer(scrollbarGraphicsLayer);
            return;
        }

        WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, orientation);
        if (!scrollbarLayer)
            scrollbarLayer = addWebScrollbarLayer(scrollableArea, orientation, createScrollbarLayer(scrollbar));

        // Root layer non-overlay scrollbars should be marked opaque to disable
        // blending.
        bool isOpaqueScrollbar = !scrollbar->isOverlayScrollbar();
        if (!scrollbarGraphicsLayer->contentsOpaque())
            scrollbarGraphicsLayer->setContentsOpaque(isMainFrame && isOpaqueScrollbar);
        scrollbarLayer->layer()->setOpaque(scrollbarGraphicsLayer->contentsOpaque());

        setupScrollbarLayer(scrollbarGraphicsLayer, scrollbarLayer, scrollingWebLayerForScrollableArea(scrollableArea));
    } else
        removeWebScrollbarLayer(scrollableArea, orientation);
}

void ScrollingCoordinator::scrollableAreaScrollLayerDidChange(ScrollableArea* scrollableArea)
{
    GraphicsLayerChromium* scrollLayer = static_cast<GraphicsLayerChromium*>(scrollLayerForScrollableArea(scrollableArea));
    if (scrollLayer)
        scrollLayer->setScrollableArea(scrollableArea);

    WebLayer* webLayer = scrollingWebLayerForScrollableArea(scrollableArea);
    if (webLayer) {
        webLayer->setScrollable(true);
        webLayer->setScrollPosition(IntPoint(scrollableArea->scrollPosition() - scrollableArea->minimumScrollPosition()));
        webLayer->setMaxScrollPosition(IntSize(scrollableArea->scrollSize(HorizontalScrollbar), scrollableArea->scrollSize(VerticalScrollbar)));
    }
    if (WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, HorizontalScrollbar))
        setupScrollbarLayer(horizontalScrollbarLayerForScrollableArea(scrollableArea), scrollbarLayer, webLayer);
    if (WebScrollbarLayer* scrollbarLayer = getWebScrollbarLayer(scrollableArea, VerticalScrollbar))
        setupScrollbarLayer(verticalScrollbarLayerForScrollableArea(scrollableArea), scrollbarLayer, webLayer);
}

void ScrollingCoordinator::setTouchEventTargetRects(const Vector<IntRect>& absoluteHitTestRects)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view())) {
        WebVector<WebRect> webRects(absoluteHitTestRects.size());
        for (size_t i = 0; i < absoluteHitTestRects.size(); ++i)
            webRects[i] = absoluteHitTestRects[i];
        scrollLayer->setTouchEventHandlerRegion(webRects);
    }
}

void ScrollingCoordinator::touchEventTargetRectsDidChange(const Document*)
{
    // The rects are always evaluated and used in the main frame coordinates.
    FrameView* frameView = m_page->mainFrame()->view();
    Document* document = m_page->mainFrame()->document();

    // Wait until after layout to update.
    if (frameView->needsLayout() || !document)
        return;

    Vector<IntRect> touchEventTargetRects;
    computeAbsoluteTouchEventTargetRects(document, touchEventTargetRects);
    setTouchEventTargetRects(touchEventTargetRects);
}

void ScrollingCoordinator::setWheelEventHandlerCount(unsigned count)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view()))
        scrollLayer->setHaveWheelEventHandlers(count > 0);
}

void ScrollingCoordinator::recomputeWheelEventHandlerCountForFrameView(FrameView* frameView)
{
    UNUSED_PARAM(frameView);
    setWheelEventHandlerCount(computeCurrentWheelEventHandlerCount());
}

void ScrollingCoordinator::setShouldUpdateScrollLayerPositionOnMainThread(MainThreadScrollingReasons reasons)
{
    if (WebLayer* scrollLayer = scrollingWebLayerForScrollableArea(m_page->mainFrame()->view()))
        scrollLayer->setShouldScrollOnMainThread(reasons);
}

void ScrollingCoordinator::pageDestroyed()
{
    ASSERT(m_page);
    m_page = 0;
}

bool ScrollingCoordinator::coordinatesScrollingForFrameView(FrameView* frameView) const
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    // We currently only handle the main frame.
    if (frameView->frame() != m_page->mainFrame())
        return false;

    // We currently only support composited mode.
    RenderView* renderView = m_page->mainFrame()->contentRenderer();
    if (!renderView)
        return false;
    return renderView->usesCompositing();
}

Region ScrollingCoordinator::computeShouldHandleScrollGestureOnMainThreadRegion(const Frame* frame, const IntPoint& frameLocation) const
{
    Region shouldHandleScrollGestureOnMainThreadRegion;
    FrameView* frameView = frame->view();
    if (!frameView)
        return shouldHandleScrollGestureOnMainThreadRegion;

    IntPoint offset = frameLocation;
    offset.moveBy(frameView->frameRect().location());

    if (const FrameView::ScrollableAreaSet* scrollableAreas = frameView->scrollableAreas()) {
        for (FrameView::ScrollableAreaSet::const_iterator it = scrollableAreas->begin(), end = scrollableAreas->end(); it != end; ++it) {
            ScrollableArea* scrollableArea = *it;
            // Composited scrollable areas can be scrolled off the main thread.
            if (scrollableArea->usesCompositedScrolling())
                continue;
            IntRect box = scrollableArea->scrollableAreaBoundingBox();
            box.moveBy(offset);
            shouldHandleScrollGestureOnMainThreadRegion.unite(box);
        }
    }

    // We use GestureScrollBegin/Update/End for moving the resizer handle. So we mark these
    // small resizer areas as non-fast-scrollable to allow the scroll gestures to be passed to
    // main thread if they are targeting the resizer area. (Resizing is done in EventHandler.cpp
    // on main thread).
    if (const FrameView::ResizerAreaSet* resizerAreas = frameView->resizerAreas()) {
        for (FrameView::ResizerAreaSet::const_iterator it = resizerAreas->begin(), end = resizerAreas->end(); it != end; ++it) {
            RenderLayer* layer = *it;
            IntRect bounds = layer->renderer()->absoluteBoundingBoxRect();
            IntRect corner = layer->resizerCornerRect(bounds, RenderLayer::ResizerForTouch);
            corner.moveBy(offset);
            shouldHandleScrollGestureOnMainThreadRegion.unite(corner);
        }
    }

    if (const HashSet<RefPtr<Widget> >* children = frameView->children()) {
        for (HashSet<RefPtr<Widget> >::const_iterator it = children->begin(), end = children->end(); it != end; ++it) {
            if (!(*it)->isPluginView())
                continue;

            PluginView* pluginView = toPluginView((*it).get());
            if (pluginView->wantsWheelEvents())
                shouldHandleScrollGestureOnMainThreadRegion.unite(pluginView->frameRect());
        }
    }

    FrameTree* tree = frame->tree();
    for (Frame* subFrame = tree->firstChild(); subFrame; subFrame = subFrame->tree()->nextSibling())
        shouldHandleScrollGestureOnMainThreadRegion.unite(computeShouldHandleScrollGestureOnMainThreadRegion(subFrame, offset));

    return shouldHandleScrollGestureOnMainThreadRegion;
}

static void accumulateRendererTouchEventTargetRects(Vector<IntRect>& rects, const RenderObject* renderer, const IntRect& parentRect = IntRect())
{
    IntRect adjustedParentRect = parentRect;
    if (parentRect.isEmpty() || renderer->isFloating() || renderer->isPositioned() || renderer->hasTransform()) {
        // FIXME: This method is O(N^2) as it walks the tree to the root for every renderer. RenderGeometryMap would fix this.
        IntRect r = enclosingIntRect(renderer->clippedOverflowRectForRepaint(0));
        if (!r.isEmpty()) {
            // Convert to the top-level view's coordinates.
            ASSERT(renderer->document()->view());
            r = renderer->document()->view()->convertToRootView(r);

            if (!parentRect.contains(r)) {
                rects.append(r);
                adjustedParentRect = r;
            }
        }
    }

    for (RenderObject* child = renderer->firstChild(); child; child = child->nextSibling())
        accumulateRendererTouchEventTargetRects(rects, child, adjustedParentRect);
}

static void accumulateDocumentEventTargetRects(Vector<IntRect>& rects, const Document* document)
{
    ASSERT(document);
    if (!document->touchEventTargets())
        return;

    const TouchEventTargetSet* targets = document->touchEventTargets();
    for (TouchEventTargetSet::const_iterator iter = targets->begin(); iter != targets->end(); ++iter) {
        const Node* touchTarget = iter->key;
        if (!touchTarget->inDocument())
            continue;

        if (touchTarget == document) {
            if (RenderView* view = document->renderView()) {
                IntRect r;
                if (touchTarget == document->topDocument())
                    r = view->documentRect();
                else
                    r = enclosingIntRect(view->clippedOverflowRectForRepaint(0));

                if (!r.isEmpty()) {
                    ASSERT(view->document()->view());
                    r = view->document()->view()->convertToRootView(r);
                    rects.append(r);
                }
            }
            return;
        }

        if (touchTarget->isDocumentNode() && touchTarget != document) {
            accumulateDocumentEventTargetRects(rects, toDocument(touchTarget));
            continue;
        }

        if (RenderObject* renderer = touchTarget->renderer())
            accumulateRendererTouchEventTargetRects(rects, renderer);
    }
}

void ScrollingCoordinator::computeAbsoluteTouchEventTargetRects(const Document* document, Vector<IntRect>& rects)
{
    ASSERT(document);
    if (!document->view())
        return;

    // FIXME: These rects won't be properly updated if the renderers are in a sub-tree that scrolls.
    accumulateDocumentEventTargetRects(rects, document);
}

unsigned ScrollingCoordinator::computeCurrentWheelEventHandlerCount()
{
    unsigned wheelEventHandlerCount = 0;

    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            wheelEventHandlerCount += frame->document()->wheelEventHandlerCount();
    }

    return wheelEventHandlerCount;
}

void ScrollingCoordinator::frameViewWheelEventHandlerCountChanged(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    recomputeWheelEventHandlerCountForFrameView(frameView);
}

void ScrollingCoordinator::frameViewHasSlowRepaintObjectsDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateShouldUpdateScrollLayerPositionOnMainThread();
}

void ScrollingCoordinator::frameViewFixedObjectsDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    updateShouldUpdateScrollLayerPositionOnMainThread();
}

GraphicsLayer* ScrollingCoordinator::scrollLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForScrolling();
}

GraphicsLayer* ScrollingCoordinator::horizontalScrollbarLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForHorizontalScrollbar();
}

GraphicsLayer* ScrollingCoordinator::verticalScrollbarLayerForScrollableArea(ScrollableArea* scrollableArea)
{
    return scrollableArea->layerForVerticalScrollbar();
}

GraphicsLayer* ScrollingCoordinator::scrollLayerForFrameView(FrameView* frameView)
{
    Frame* frame = frameView->frame();
    if (!frame)
        return 0;

    RenderView* renderView = frame->contentRenderer();
    if (!renderView)
        return 0;
    return renderView->compositor()->scrollLayer();
}

GraphicsLayer* ScrollingCoordinator::counterScrollingLayerForFrameView(FrameView*)
{
    return 0;
}

void ScrollingCoordinator::frameViewRootLayerDidChange(FrameView* frameView)
{
    ASSERT(isMainThread());
    ASSERT(m_page);

    if (!coordinatesScrollingForFrameView(frameView))
        return;

    frameViewLayoutUpdated(frameView);
    recomputeWheelEventHandlerCountForFrameView(frameView);
    updateShouldUpdateScrollLayerPositionOnMainThread();
}

#if OS(DARWIN)
void ScrollingCoordinator::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    ASSERT(isMainThread());

    if (!m_page)
        return;

    FrameView* frameView = m_page->mainFrame()->view();
    if (!frameView)
        return;

    frameView->scrollAnimator()->handleWheelEventPhase(phase);
}
#endif

bool ScrollingCoordinator::hasVisibleSlowRepaintViewportConstrainedObjects(FrameView* frameView) const
{
    const FrameView::ViewportConstrainedObjectSet* viewportConstrainedObjects = frameView->viewportConstrainedObjects();
    if (!viewportConstrainedObjects)
        return false;

    for (FrameView::ViewportConstrainedObjectSet::const_iterator it = viewportConstrainedObjects->begin(), end = viewportConstrainedObjects->end(); it != end; ++it) {
        RenderObject* viewportConstrainedObject = *it;
        if (!viewportConstrainedObject->isBoxModelObject() || !viewportConstrainedObject->hasLayer())
            return true;
        RenderLayer* layer = toRenderBoxModelObject(viewportConstrainedObject)->layer();
        // Any explicit reason that a fixed position element is not composited shouldn't cause slow scrolling.
        if (!layer->isComposited() && layer->viewportConstrainedNotCompositedReason() == RenderLayer::NoNotCompositedReason)
            return true;
    }
    return false;
}

MainThreadScrollingReasons ScrollingCoordinator::mainThreadScrollingReasons() const
{
    FrameView* frameView = m_page->mainFrame()->view();
    if (!frameView)
        return static_cast<MainThreadScrollingReasons>(0);

    MainThreadScrollingReasons mainThreadScrollingReasons = (MainThreadScrollingReasons)0;

    if (frameView->hasSlowRepaintObjects())
        mainThreadScrollingReasons |= HasSlowRepaintObjects;
    if (hasVisibleSlowRepaintViewportConstrainedObjects(frameView))
        mainThreadScrollingReasons |= HasNonLayerViewportConstrainedObjects;

    return mainThreadScrollingReasons;
}

void ScrollingCoordinator::updateShouldUpdateScrollLayerPositionOnMainThread()
{
    setShouldUpdateScrollLayerPositionOnMainThread(mainThreadScrollingReasons());
}

String ScrollingCoordinator::mainThreadScrollingReasonsAsText(MainThreadScrollingReasons reasons)
{
    StringBuilder stringBuilder;

    if (reasons & ScrollingCoordinator::HasSlowRepaintObjects)
        stringBuilder.append("Has slow repaint objects, ");
    if (reasons & ScrollingCoordinator::HasViewportConstrainedObjectsWithoutSupportingFixedLayers)
        stringBuilder.append("Has viewport constrained objects without supporting fixed layers, ");
    if (reasons & ScrollingCoordinator::HasNonLayerViewportConstrainedObjects)
        stringBuilder.append("Has non-layer viewport-constrained objects, ");

    if (stringBuilder.length())
        stringBuilder.resize(stringBuilder.length() - 2);
    return stringBuilder.toString();
}

String ScrollingCoordinator::mainThreadScrollingReasonsAsText() const
{
    return mainThreadScrollingReasonsAsText(mainThreadScrollingReasons());
}

} // namespace WebCore
